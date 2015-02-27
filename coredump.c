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

	out = snprintf(path, sizeof(path), "/run/crash/%s", argv[1]);
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
