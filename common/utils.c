#include <unistd.h>

#include <stdlib.h>
#include <stdio.h>

#include "utils.h"

void file_cleanup(FILE **f)
{
	if (f == NULL || *f == NULL)
		return;

	fclose(*f);
	*f = NULL;
}

void string_cleanup(char **s)
{
	if (s == NULL || *s == NULL)
		return;

	free(*s);
	*s = NULL;
}

void fd_cleanup(int *fd)
{
	if (fd == NULL)
		return;

	close(*fd);
	*fd = -1;
}
