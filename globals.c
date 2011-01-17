/*
 * jpg-recover -- Extract JPEG/CR2 files from raw bytes (filesystem images etc.).
 * Copyright (c) 2011, Matus Tejiscak <functor.sk@ziman>.
 * Released under the BSD license: http://creativecommons.org/licenses/BSD/
 */
#include <stdio.h>
#include <stdlib.h>

#include "globals.h"

void die(const char * msg)
{
	fprintf(stderr, "Error: %s\nAborting.\n", msg);
	exit(1);
}
