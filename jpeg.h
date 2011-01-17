/*
 * jpg-recover -- Extract JPEG/CR2 files from raw bytes (filesystem images etc.).
 * Copyright (c) 2011, Matus Tejiscak <functor.sk@ziman>.
 * Released under the BSD license: http://creativecommons.org/licenses/BSD/
 */
#ifndef JPEG_H
#define JPEG_H

#include <stdio.h>

#include "globals.h"

/**
 * Try to recover a JPEG file from the current position in the stream.
 * @param f The input stream
 * @param index The index used to generate the output file name.
 * @param requireE0E1 Require an E0 or E1 marker at the beginning of the file.
 * @param prefix The prefix used to generate the names of the recovered files.
 * @return The next index if successful, the same index if unsuccessful.
 */
int recoverJpeg(FILE * f, int index, bool_t requireE0E1, const char * prefix);

#endif
