#include <pwd.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>

#define USAGE "Usage: %s\n"

int main(int argc, char *argv[])
{
	struct passwd *user;

	if (1 != argc) {
		(void) fprintf(stderr, USAGE, argv[0]);
		goto end;
	}

	user = getpwuid(geteuid());
	if (NULL == user)
		goto end;

	if (-1 == setenv("USER", user->pw_name, 1))
		goto end;
	if (-1 == setenv("HOME", user->pw_dir, 1))
		goto end;
	if (-1 == setenv("SHELL", user->pw_shell, 1))
		goto end;

	if (-1 == chdir(user->pw_dir))
		goto end;

	(void) execlp(user->pw_shell, user->pw_shell, (char *) NULL);

end:
	return EXIT_FAILURE;
}
