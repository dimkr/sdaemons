#include <stdlib.h>
#include <stdio.h>
#include <syslog.h>
#include <sys/klog.h>

#define PROG "klog"

#define MAX_KMSG_LEN (1024)

#define USAGE "Usage: %s\n"

int main(int argc, char *argv[]) {
	char buf[MAX_KMSG_LEN];
	int len;
	int priority ;
	unsigned int off;

	if (1 != argc) {
		(void) fprintf(stderr, USAGE, argv[0]);
		goto end;
	}

	if (-1 == klogctl(1, NULL, 0))
		goto end;

	/* disable echo of messages to the console */
	if (-1 == klogctl(6, NULL, 0))
		goto close_klog;

	openlog(PROG, LOG_NDELAY, LOG_KERN);

	do {
		len = klogctl(2, buf, sizeof(buf));
		switch (len) {
			case (-1):
				goto close_syslog;

			case 0:
				continue;
		}

		/* parse the message priority */
		priority = LOG_INFO;
		if (3 >= len) {
			off = 0;
		} else {
			if (('<' == buf[0]) && ('>' == buf[2])) {
				priority = buf[1] - '0';
				off = 3;
			}
			else
				off = 0;
		}

		buf[len] = '\0';

		syslog(priority, "%s", buf + off);
	} while (1);

close_syslog:
	closelog();

	(void) klogctl(7, NULL, 0);

close_klog:
	(void) klogctl(0, NULL, 0);

end:
	/* we never return EXIT_SUCCESS; we cannot handle signals because klogctl()
	 * is blocking */
	return EXIT_FAILURE;
}
