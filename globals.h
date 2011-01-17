#ifndef GLOBALS_H
#define GLOBALS_H

#include <stdint.h>

/** An explicit boolean type. */
typedef int bool_t;
#define false 0
#define true 1

/** Maximum prefix length (the "recovered" part from recoveredXXXXX). */
#define MAX_PREFIX_LENGTH 256

/** An unsigned byte, to save typing. */
typedef uint8_t uchar;

/** Print a fatal error and exit. */
void die(const char * msg);

#endif