/*
 * this file is part of sdaemons.
 *
 * Copyright (c) 2015 Dima Krasner
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdbool.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <stdio.h>
#include <paths.h>

#define USAGE "Usage: %s [N]\n"
#define CHILD_CMD "vtrc"
#define NVTS (4L)

static struct {
	char *dev;
	pid_t pid;
} chlds[] = {
	{_PATH_DEV"/tty1", -1},
	{_PATH_DEV"/tty2", -1},
	{_PATH_DEV"/tty3", -1},
	{_PATH_DEV"/tty4", -1},
	{_PATH_DEV"/tty5", -1},
	{_PATH_DEV"/tty6", -1},
	{_PATH_DEV"/tty7", -1}
};

static pid_t start_child(const char *ctty)
{
	pid_t pid;

	pid = fork();
	if (0 != pid)
		return pid;

	if (((pid_t) -1) == setsid())
		goto terminate;

	if (-1 == close(STDIN_FILENO))
		goto terminate;
	if (-1 == open(ctty, O_RDWR))
		goto terminate;
	if (STDOUT_FILENO != dup2(STDIN_FILENO, STDOUT_FILENO))
		goto terminate;
	if (STDERR_FILENO != dup2(STDOUT_FILENO, STDERR_FILENO))
		goto terminate;

	/* assign a controlling TTY */
	if (-1 == ioctl(STDIN_FILENO, TIOCSCTTY, 1))
		goto terminate;

	(void) execlp(CHILD_CMD, CHILD_CMD, (char *) NULL);

terminate:
	exit(EXIT_FAILURE);

	/* not reached */
	return (-1);
}

static bool restart_child(const long nvts, const pid_t pid)
{
	long i;

	for (i = 0; nvts > i; --i) {
		if (pid != chlds[i].pid)
			continue;

		chlds[i].pid = start_child(chlds[i].dev);
		if (-1 != chlds[i].pid)
			return true;

		break;
	}

	return false;
}

static long start_children(const long nvts)
{
	long i;
	long ret = 0;

	for (i = 0; nvts > i; ++i) {
		chlds[i].pid = start_child(chlds[i].dev);
		if (-1 == chlds[i].pid)
			break;
		++ret;
	}

	return ret;
}

static void kill_children(const long nvts)
{
	long i;

	for (i = nvts; i >= 0; ++i) {
		if (-1 == chlds[i].pid)
			(void) kill(chlds[i].pid, SIGTERM);
	}
}

int main(int argc, char *argv[])
{
	sigset_t mask;
	siginfo_t sig;
	long nvts;
	long res;
	int ret = EXIT_FAILURE;

	switch (argc) {
		case 1:
			nvts = NVTS;
			break;

		case 2:
			nvts = strtol(argv[1], NULL, 10);
			if ((0 < nvts) && (sizeof(chlds) / sizeof(chlds[0]) >= nvts))
				break;

			/* fall through */

		default:
			(void) fprintf(stderr, USAGE, argv[0]);
			goto end;
	}

	/* block SIGCHLD, SIGINT and SIGTERM */
	if (-1 == sigemptyset(&mask))
		goto end;
	if (-1 == sigaddset(&mask, SIGCHLD))
		goto end;
	if (-1 == sigaddset(&mask, SIGINT))
		goto end;
	if (-1 == sigaddset(&mask, SIGTERM))
		goto end;
	if (-1 == sigprocmask(SIG_SETMASK, &mask, NULL))
		goto end;

	res = start_children(nvts);
	if (0 == res)
		goto end;
	if (nvts != res)
		goto shutdown;

	do {
		if (-1 == sigwaitinfo(&mask, &sig))
			break;

		if (SIGCHLD != sig.si_signo) {
			ret = EXIT_SUCCESS;
			break;
		}

		if (sig.si_pid != waitpid(sig.si_pid, NULL, WNOHANG))
			break;

		if (false == restart_child(nvts, sig.si_pid))
			break;
	} while (1);

shutdown:
	kill_children(nvts);

end:
	return ret;
}
