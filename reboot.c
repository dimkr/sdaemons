#include <signal.h>
#include <stdio.h>
#include <sys/types.h>
#include <signal.h>
#include <stdlib.h>

#define USAGE "Usage: %s\n"

int main(int argc, char *argv[])
{
	if (1 != argc) {
		(void) fprintf(stderr, USAGE, argv[0]);
		return EXIT_FAILURE;
	}

	if (-1 == kill(1, SIGUSR2))
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}
