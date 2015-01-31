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

#define USAGE "Usage: %s ARG...\n"

static unsigned char stack[8 * 1024];

static int routine(void *arg)
{
	siginfo_t sig;
	sigset_t mask;
	char **argv = (char **) arg;
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
	if (-1 == mount("proc", "/proc", "proc", 0UL, NULL))
		goto end;

	pid = fork();
	switch (pid) {
		case 0:
			if (-1 == sigemptyset(&mask))
				goto terminate;
			if (-1 == sigprocmask(SIG_SETMASK, &mask, NULL))
				goto terminate;

			(void) execvp(argv[0], argv);

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
	pid_t pid;
	int status;
	int sig;

	if (1 >= argc) {
		(void) fprintf(stderr, USAGE, argv[0]);
		return EXIT_FAILURE;
	}

	/* for extra portability (i.e architectures where the stack grows in both
	 * directions), we point at the stack middle */
	pid = clone(routine,
	            stack + (sizeof(stack) / 2),
	            CLONE_NEWPID | CLONE_NEWNS | SIGCHLD,
	            (void *) &argv[1]);
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
}
