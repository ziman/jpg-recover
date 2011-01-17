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

void dumpFile(FILE * f, const char * const fname, unsigned size)
{
	FILE * out = fopen(fname, "wb");
	if (!out) {
		perror(fname);
		die("Could not dump recovered file.");
	}

	while (size > 0) {
		uint8_t buffer[512 * 1024]; /* 0.5MB buffer for copying */
		unsigned toRead = (size < sizeof(buffer)) ? size : sizeof(buffer);
		unsigned bytes = fread(buffer, 1, toRead, f);
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

/** Return the size of an element of the type, in bytes. */
unsigned typeSize(unsigned type)
{
	switch (type) {
		case 1: return 1; /* BYTE */
		case 2: return 1; /* ASCII */
		case 3: return 2; /* SHORT */
		case 4: return 4; /* LONG */
		case 5: return 8; /* RATIONAL (= 2 LONGs) */
		default:
			printf("  ! Warning, unrecognized TIFF entry type: %d, assuming size of zero.\n", type);
			printf("  ! The recovered image may be damaged.\n");
			return 0;
	}
}

int recoverTiff(FILE * f, int index, bool_t bigEndian, const char * prefix)
{
	unsigned magic = readShort(f, bigEndian);
	if (magic != 42)
		return index;

	printf("TIFF file header... reading on.\n");

	unsigned tiffSize = 0;
	off_t fileStart = ftell(f) - 4;
	do {
		unsigned ifd = readLong(f, bigEndian);
		if (ifd == 0) break;

		fseek(f, fileStart + ifd, SEEK_SET);
		unsigned entryCount = readShort(f, bigEndian);
		printf("  * IF directory at offset %u, %u entries.\n", ifd, entryCount);
		while (entryCount-- > 0) {
			/* Read the entry. */
			unsigned tag = readShort(f, bigEndian);
			unsigned type = readShort(f, bigEndian);
			unsigned count = readLong(f, bigEndian);
			unsigned offset = readLong(f, bigEndian);
			unsigned blockSize = count * typeSize(type);
			unsigned blockEndOffset = offset + blockSize;

			if (blockEndOffset > tiffSize)
				tiffSize = blockEndOffset;

			printf("    -> tag = %u, type = %u, count = %u, offset = %u\n", tag, type, count, offset);
			printf("       yielding a block of %u bytes spanning %u..%u\n", blockSize, offset, blockEndOffset);
		}
	} while (true);

	char fname[MAX_PREFIX_LENGTH + 10];
	snprintf(fname, sizeof(fname), "%s%05d.cr2", prefix, index);
	fseek(f, fileStart, SEEK_SET);

	printf("The TIFF file appears correct, dumping %d bytes as %s... ", tiffSize, fname);
	fflush(stdout);

	dumpFile(f, fname, tiffSize);

	printf("done.\n");

	return index + 1;
}

