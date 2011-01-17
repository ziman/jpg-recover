#include <stdio.h>
#include <stdlib.h>

#include "globals.h"

void die(const char * msg)
{
	fprintf(stderr, "Error: %s\nAborting.\n", msg);
	exit(1);
}