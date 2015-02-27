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

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <syslog.h>

#define USAGE "Usage: %s NAME\n"
#define PATH_RUN "/run"

int main(int argc, char *argv[])
{
	char path[PATH_MAX];
	char buf[BUFSIZ];
	ssize_t len;
	int fd;
	int out;
	int ret = EXIT_FAILURE;

	if (2 != argc)
		goto usage;

	if (NULL != strchr(argv[1], '/'))
		goto usage;

	out = snprintf(path, sizeof(path), PATH_RUN"/crash/%s", argv[1]);
	if ((0 >= out) || (sizeof(path) <= out))
		goto end;

	(void) umask(0);

	fd = open(path,
	          O_WRONLY | O_CREAT | O_EXCL,
	          S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if (-1 == fd)
		goto end;

	openlog("coredump", LOG_NDELAY | LOG_PID, LOG_USER);
	syslog(LOG_INFO, "dumping to %s", path);

	do {
		len = read(STDIN_FILENO, (void *) buf, sizeof(buf));
		switch (len) {
			case 0:
				ret = EXIT_SUCCESS;

			case (-1):
				goto close_log;
		}

		if (len != write(fd, (void *) buf, (size_t) len))
			goto close_log;
	} while (1);

close_log:
	closelog();

	(void) close(fd);
	if (EXIT_FAILURE == ret)
		(void) unlink(path);

end:
	return ret;

usage:
	(void) fprintf(stderr, USAGE, argv[0]);
	return ret;
}
