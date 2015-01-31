#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#define MAX_MSG_LEN (1024)

#define PATH_DEVLOG "/dev/log"

#define PATH_SYSLOG "/var/log/messages"

#define USAGE "Usage: %s\n"

int main(int argc, char *argv[])
{
	char buf[MAX_MSG_LEN];
	struct sockaddr_un addr;
	sigset_t mask;
	ssize_t len;
	ssize_t out;
	int log_fd;
	int sock;
	int sig;
	int io_sig;
	int flags;
	int exit_code = EXIT_FAILURE;

	if (1 != argc) {
		(void) fprintf(stderr, USAGE, argv[0]);
		goto end;
	}

	io_sig = SIGRTMIN;

	if (-1 == sigemptyset(&mask))
		goto end;
	if (-1 == sigaddset(&mask, io_sig))
		goto end;
	if (-1 == sigaddset(&mask, SIGTERM))
		goto end;
	if (-1 == sigprocmask(SIG_SETMASK, &mask, NULL))
		goto end;

	log_fd = open(PATH_SYSLOG,
	              O_WRONLY | O_APPEND | O_CREAT,
	              S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if (-1 == log_fd)
		goto end;

	sock = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (-1 == sock)
		goto close_log;

	addr.sun_family = AF_UNIX;
	(void) strcpy(addr.sun_path, PATH_DEVLOG);
	if (-1 == bind(sock,
	               (struct sockaddr *) &addr,
	               sizeof(addr)))
		goto close_sock;

	if (-1 == fcntl(sock, F_SETSIG, io_sig))
 		goto close_sock;

	flags = fcntl(sock, F_GETFL);
	if (-1 == flags)
		goto close_sock;
	if (-1 == fcntl(sock, F_SETFL, flags | O_NONBLOCK | O_ASYNC))
		goto close_sock;
	if (-1 == fcntl(sock, F_SETOWN, getpid()))
		goto close_sock;

	do {
		if (0 != sigwait(&mask, &sig))
			break;

		if (SIGTERM == sig) {
			exit_code = EXIT_SUCCESS;
			break;
		}

		len = recv(sock, buf, sizeof(buf) - 2, 0);
		switch (len) {
			case (-1):
				if (EAGAIN != errno)
					goto close_sock;

				/* fall through */

			case 0:
				continue;
		}

		/* make sure the message ends with a line break */
		if ('\n' != buf[len - 1]) {
			buf[len] = '\n';
			buf[1 + len] = '\0';
			++len;
		}

		do {
			out = write(log_fd, buf, (size_t) len);
			if (-1 == out)
				break;
			len -= out;
		} while (0 < len);
	} while (1);

close_sock:
	(void) close(sock);
	(void) unlink(PATH_DEVLOG);

close_log:
	(void) close(log_fd);

end:
	return exit_code;
}
