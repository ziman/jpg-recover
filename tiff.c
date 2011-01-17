#include <stdio.h>
#include <stdlib.h>

#include "tiff.h"

/** Read an unsigned 16-bit integer from the file. */
inline unsigned readShort(FILE * f, bool_t bigEndian)
{
	if (bigEndian)
		return (fgetc(f) << 8) | fgetc(f);
	else
		return fgetc(f) | (fgetc(f) << 8);
}

/** Read an unsigned 32-bit integer from the file. */
inline unsigned readLong(FILE * f, bool_t bigEndian)
{
	if (bigEndian)
		return (readShort(f, bigEndian) << 16) | readShort(f, bigEndian);
	else
		return readShort(f, bigEndian) | (readShort(f, bigEndian) << 16);
}

/** Create a new file, copying <i>size</i> bytes from the input file into it. */
void dumpFile(FILE * f, const char * const fname, unsigned size)
{
	/* Try to open the file. */
	FILE * out = fopen(fname, "wb");
	if (!out) {
		perror(fname);
		die("Could not dump recovered file.");
	}

	/* Copy the data. */
	while (size > 0) {
		uint8_t buffer[512 * 1024]; /* 0.5MB */
		unsigned toRead = (size < sizeof(buffer)) ? size : sizeof(buffer);
		unsigned bytes = fread(buffer, 1, toRead, f);
		if (bytes == 0)
			die("Error reading input, fread returned 0.");
		
		fwrite(buffer, 1, bytes, out);
		size -= bytes;
	}

	/* Clean up. */
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
		/* Read the IF directory offset. */
		unsigned ifd = readLong(f, bigEndian);
		if (ifd == 0) break;

		/* Rewind to the directory. */
		fseek(f, fileStart + ifd, SEEK_SET);

		/* Get the entry count. */
		unsigned entryCount = readShort(f, bigEndian);
		printf("  * IF directory at offset %u, %u entries.\n", ifd, entryCount);

		/* Read all entries. */
		while (entryCount-- > 0) {
			unsigned tag = readShort(f, bigEndian);
			unsigned type = readShort(f, bigEndian);
			unsigned count = readLong(f, bigEndian);
			unsigned offset = readLong(f, bigEndian);
			unsigned blockSize = count * typeSize(type);
			unsigned blockEndOffset = offset + blockSize;

			/* A block may constitute the last bytes of a TIFF file, according to the spec. */
			if (blockEndOffset > tiffSize)
				tiffSize = blockEndOffset;

			/* Process known IFD entries. */
			switch (tag) {
				case 273: /* Strip offsets. */
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
			} /* End of the switch. */
		} /* End of the loop over entries in an IFD. */
	} while (true); /* End of the loop over IFDs. */

	/* Check whether we have any strips at all. */
	if (!stripOffsets || !stripLengths || !stripCount) {
		printf("-> Strip offsets/lengths/count not present, this file would be unusable. Skipping.\n");
		return index;
	}

	/* Usually end of the last strip is the end of the whole TIFF file. */
	unsigned lastStripEnd = 0;
	if (stripCount == 1) {
		/* Here we have exactly one strip, with actual values instead of pointers to arrays. */
		lastStripEnd = stripOffsets + stripLengths;
	} else {
		/* Multiple strips, iterate over each one to find the highest offset. */
		unsigned highestOffset = 0;
		int highestOffsetIndex = 0;
		fseek(f, fileStart + stripOffsets, SEEK_SET);
		int i;
		for (i = 0; i < stripCount; ++i) {
			/* Read the offset of the strip. */
			unsigned offset = readLong(f, bigEndian);
			if (offset > highestOffset) {
				highestOffset = offset;
				highestOffsetIndex = i;
			}
		}

		/* Now, reach into the STRIP_LENGTHS list and get the length of the last strip. */
		fseek(f, fileStart + stripLengths + 4*highestOffsetIndex, SEEK_SET);
		lastStripEnd = highestOffset + readLong(f, bigEndian);
	}

	/* Print some nice info. */
	printf("  * Strip data ends at the offset %u.\n", lastStripEnd);
	fflush(stdout);

	/* Adjust the calculated TIFF size. */
	if (lastStripEnd > tiffSize)
		tiffSize = lastStripEnd;

	/* Generate the output file name. */
	char fname[MAX_PREFIX_LENGTH + 10];
	snprintf(fname, sizeof(fname), "%s%05d.cr2", prefix, index);

	/* Seek to the beginning of the TIFF file and dump it.*/
	printf("-> The TIFF file appears correct, dumping %u bytes as %s... ", tiffSize, fname);
	fseek(f, fileStart, SEEK_SET);
	dumpFile(f, fname, tiffSize);
	printf("done.\n");

	/* Use the next index for the next image. */
	return index + 1;
}

