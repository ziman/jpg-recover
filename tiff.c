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
	/* Check for the correct magic code. */
	unsigned magic = readShort(f, bigEndian);
	if (magic != 42)
		return index;

	printf("Correct TIFF file header recognized... reading on.\n");

	/* These values will be gathered while reading the file. */
	unsigned tiffSize = 0;
	unsigned stripOffsets = 0;
	unsigned stripLengths = 0;
	unsigned stripCount = 0;

	/* Parse the directories. */
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

			printf("    - tag = %u, type = %u, count = %u, offset = %u\n", tag, type, count, offset);
			printf("      yielding a block of %u bytes spanning %u..%u\n", blockSize, offset, blockEndOffset);

			switch (tag) {
				case 273: /* Strip offsets */
					stripOffsets = offset;
					if (type != 4) {
						printf("-> STRIP_OFFSETS are not LONGs. Skipping.\n");
						return index;
					}
					if (stripCount && stripCount != count) {
						printf("  ! Warning: STRIP_OFFSETS has different count of elements than STRIP_LENGTHS.\n");
						printf("  !          The resulting file may be unusable.\n");
						stripCount = (stripCount > count) ? count : stripCount;
					} else {
						stripCount = count;
					}
					break;
				case 279: /* Strip byte counts */
					stripLengths = offset;
					if (type != 4) {
						printf("-> STRIP_LENGTHS are not LONGs. Skipping.\n");
						return index;
					}
					if (stripCount && stripCount != count) {
						printf("  ! Warning: STRIP_OFFSETS has different count of elements than STRIP_LENGTHS.\n");
						printf("  !          The resulting file may be unusable.\n");
						stripCount = (stripCount > count) ? count : stripCount;
					} else {
						stripCount = count;
					}
					break;
				default: /* Unrecognized tag, ignore it. */
					break;
			}
		}
	} while (true);

	if (!stripOffsets || !stripLengths) {
		printf("-> Strip offsets or lengths not present, this file would be unusable. Skipping.\n");
		return index;
	}

	if (stripCount == 1) {
		/* Here we have exactly one strip, with actual values instead of pointers to arrays. */
		unsigned lastStripEnd = stripOffsets + stripLengths;
		if (lastStripEnd > tiffSize)
			tiffSize = lastStripEnd;
	} else {
		unsigned highestOffset = 0;
		int highestOffsetIndex = 0;
		fseek(f, fileStart + stripOffsets, SEEK_SET);
		int i;
		for (i = 0; i < stripCount; ++i) {
			unsigned offset = readLong(f, bigEndian);
			if (offset > highestOffset) {
				printf("[%d of %u] Setting highest offset to %u\n", i+1, stripCount, offset);
				highestOffset = offset;
				highestOffsetIndex = i;
			}
		}

		fseek(f, fileStart + stripLengths + 4*highestOffsetIndex, SEEK_SET);
		unsigned lastStripEnd = highestOffset + readLong(f, bigEndian);

		if (lastStripEnd > tiffSize)
			tiffSize = lastStripEnd;
	}

	char fname[MAX_PREFIX_LENGTH + 10];
	snprintf(fname, sizeof(fname), "%s%05d.cr2", prefix, index);
	fseek(f, fileStart, SEEK_SET);

	printf("-> The TIFF file appears correct, dumping %u bytes as %s... ", tiffSize, fname);
	fflush(stdout);

	dumpFile(f, fname, tiffSize);

	printf("done.\n");

	return index + 1;
}

