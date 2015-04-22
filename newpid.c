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

#include <pthread.h>
#include <sched.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include <sys/mount.h>
#include <errno.h>

/*
 * newpid uses three processes:
 *   - the parent, which waits for its child to terminate, then sends its exit
 *     code to init(8), as a signal
 *   - the child, which acts as an init process for the container; it catches
 *     the same signals as init(8) and terminates when a signal is received,
 *     with the signal as the exit code
 *   - the command-line provided by the user, which runs inside the container
 */

#define USAGE "Usage: %s [-n] ARG...\n"

static unsigned char stack[8 * 1024];

struct init_arg {
	char **argv;
	int flags;
};

static int routine(void *arg)
{
	siginfo_t sig;
	sigset_t mask;
	const struct init_arg *data = (const struct init_arg *) arg;
	pid_t pid;
	int ret = EXIT_FAILURE;

	if (-1 == sigemptyset(&mask))
		goto end;
	if (-1 == sigaddset(&mask, SIGUSR1))
		goto end;
	if (-1 == sigaddset(&mask, SIGUSR2))
		goto end;
	if (-1 == sigaddset(&mask, SIGCHLD))
		goto end;
	if (-1 == sigprocmask(SIG_SETMASK, &mask, NULL))
		goto end;

	/* mount /proc, so it shows only processes that belong to the namespace */
	if (0 != (CLONE_NEWPID & data->flags)) {
		if (-1 == mount("proc", "/proc", "proc", 0UL, NULL))
			goto end;
	}

	pid = fork();
	switch (pid) {
		case 0:
			if (-1 == sigemptyset(&mask))
				goto terminate;
			if (-1 == sigprocmask(SIG_SETMASK, &mask, NULL))
				goto terminate;

			(void) execvp(data->argv[0], data->argv);

terminate:
			exit(EXIT_FAILURE);

		case (-1):
			goto end;
	}

	do {
		if (-1 == sigwaitinfo(&mask, &sig))
			break;

		if (SIGCHLD != sig.si_signo) {
			ret = sig.si_signo;
			break;
		}

		if (sig.si_pid != waitpid(sig.si_pid, NULL, WNOHANG))
			break;

		/* if the child process died before poweroff(8) or reboot(8) ran,
		 * shut down by passing SIGUSR1 to init */
		if (pid == sig.si_pid) {
			ret = SIGUSR1;
			break;
		}
	} while (1);

end:
	return ret;
}

int main(int argc, char *argv[])
{
	struct init_arg data;
	pid_t pid;
	int status;
	int sig;

	if (1 >= argc)
		goto usage;

	if ('-' != argv[1][0]) {
		data.flags = CLONE_NEWPID | CLONE_NEWNS | SIGCHLD;
		data.argv = (void *) &argv[1];
	}
	else {
		if (2 >= argc)
			goto usage;
		if (('n' != argv[1][1]) || ('\0' != argv[1][2]))
			goto usage;
		data.flags = SIGCHLD;
		data.argv = (void *) &argv[2];
	}

	/* for extra portability (i.e architectures where the stack grows in both
	 * directions), we point at the stack middle */
	pid = clone(routine,
	            stack + (sizeof(stack) / 2),
	            data.flags,
	            &data);
	if (-1 == pid)
		return EXIT_FAILURE;

	if (pid != waitpid(pid, &status, 0))
		return EXIT_FAILURE;

	/* obtain the container init process exit status and use it as a signal sent
	 * to the real init */
	if (WIFEXITED(status)) {
		sig = WEXITSTATUS(status);
		if ((SIGUSR1 != sig) && (SIGUSR2 != sig))
			sig = SIGUSR1;
	} else
		sig = SIGUSR1;

	if (-1 == kill(1, sig))
		return EXIT_FAILURE;

	return EXIT_SUCCESS;

usage:
	(void) fprintf(stderr, USAGE, argv[0]);
	return EXIT_FAILURE;
}
