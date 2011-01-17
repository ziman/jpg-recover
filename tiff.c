#include <stdio.h>
#include <stdlib.h>

#include "tiff.h"

inline unsigned readShort(FILE * f, bool_t bigEndian)
{
	if (bigEndian)
		return (fgetc(f) << 8) | fgetc(f);
	else
		return fgetc(f) | (fgetc(f) << 8);
}

inline unsigned readLong(FILE * f, bool_t bigEndian)
{
	if (bigEndian)
		return (readShort(f, bigEndian) << 16) | readShort(f, bigEndian);
	else
		return readShort(f, bigEndian) | (readShort(f, bigEndian) << 16);
}

void dumpFile(FILE * f, const char * const fname, off_t size)
{
	FILE * out = fopen(fname, "wb");
	if (!out) {
		perror(fname);
		die("Could not dump recovered file.");
	}

	while (size > 0) {
		uint8_t buffer[512 * 1024]; /* 0.5MB buffer for copying */
		off_t toRead = (size < sizeof(buffer)) ? size : sizeof(buffer);
		off_t bytes = fread(buffer, 1, toRead, f);
		if (bytes == 0) {
			if (feof(f))
				break;
			printf("size = %d\n", size);
			die("Error reading input, fread returned 0.");
		}
		fwrite(buffer, 1, bytes, out);
		size -= bytes;
	}

	fclose(out);
}

/**
 * Try to recover a TIFF file from the current position in the stream.
 * @param f The input stream.
 * @param index The index used to generate the output file name.
 * @param bigEndian true iff the TIFF file is big-endian.
 * @return The next index if successful, the same index if unsuccessful.
 * @note CR2 files are structurally TIFF files.
 */
int recoverTiff(FILE * f, int index, bool_t bigEndian, const char * prefix)
{
	unsigned magic = readShort(f, bigEndian);
	if (magic != 42)
		return index;

	printf("TIFF file header... reading on.\n");

	off_t fileStart = ftell(f) - 4;
	do {
		off_t ifd = readLong(f, bigEndian);
		if (ifd == 0) break;

		fseek(f, fileStart + ifd, SEEK_SET);
		unsigned entries = readShort(f, bigEndian);
		fseek(f, 12 * entries, SEEK_CUR);

		printf("  * IF directory at offset %d, %d entries.\n", ifd, entries);
	} while (true);

	off_t fileEnd = ftell(f);

	char fname[MAX_PREFIX_LENGTH + 10];
	snprintf(fname, sizeof(fname), "%s%05d.jpg", prefix, index);
	fseek(f, fileStart, SEEK_SET);

	printf("The TIFF file appears correct, dumping %d bytes as %s... ", fileEnd-fileStart, fname);
	fflush(stdout);

	dumpFile(f, fname, fileEnd - fileStart);

	printf("done.\n");

	return index + 1;
}

