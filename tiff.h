/*
 * jpg-recover -- Extract JPEG/CR2 files from raw bytes (filesystem images etc.).
 * Copyright (c) 2011, Matus Tejiscak <functor.sk@ziman>.
 * Released under the BSD license: http://creativecommons.org/licenses/BSD/
 */
#ifndef TIFF_H
#define TIFF_H

#include <stdio.h>

#include "globals.h"

/**
 * Try to recover a TIFF file from the current position in the stream.
 * @param f The input stream.
 * @param index The index used to generate the output file name.
 * @param bigEndian true iff the TIFF file is big-endian.
 * @param prefix The prefix used to generate the names of the recovered files.
 * @return The next index if successful, the same index if unsuccessful.
 * @note CR2 files are structurally TIFF files.
 */
int recoverTiff(FILE * f, int index, bool_t bigEndian, const char * prefix);

#endif
