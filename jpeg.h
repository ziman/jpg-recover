#ifndef JPEG_H
#define JPEG_H

#include <stdio.h>

#include "globals.h"

/**
 * Try to recover a JPEG file from the current position in the stream.
 * @param f The input stream
 * @param index The index used to generate the output file name.
 * @param prefix The prefix used to generate the names of the recovered files.
 * @return The next index if successful, the same index if unsuccessful.
 */
int recoverJpeg(FILE * f, int index, const char * prefix);

#endif
