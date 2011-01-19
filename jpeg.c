/*
 * jpg-recover -- Extract JPEG/CR2 files from raw bytes (filesystem images etc.).
 * Copyright (c) 2011, Matus Tejiscak <functor.sk@ziman>.
 * Released under the BSD license: http://creativecommons.org/licenses/BSD/
 */
#include "jpeg.h"

/** Maximum size of the SOS-EOI block, in bytes. */
#define MAX_SCANLINES_SIZE 8 * 1024 * 1024

int recoverJpeg(FILE * f, int index, bool_t requireE0E1, const char * prefix)
{
	/** Are we processing the first marker? */
	int firstMarker = 1;

	/** The output file (recoveredXXXXX.jpg). */
	FILE * out = NULL;

	/** The name of the output file. */
	char fname[MAX_PREFIX_LENGTH + 10];

	/* In this position, we have already read FF D8. */

	while (!feof(f)) {
		/* Read the next marker-intro, which must be FF. */
		uchar ff = fgetc(f);
		if (ff != 0xFF) {
			if (!firstMarker)
				printf("-> quitting on invalid marker.\n");
			if (out) fclose(out);

			/* Invalid marker, reuse the index for the next file. */
			return index;
		}

		/* Read the next marker. */
		uchar marker = fgetc(f);
		if (firstMarker) {
			/* First marker, an E0 or E1 marker must follow. */
			if (requireE0E1 && marker != 0xE0 && marker != 0xE1) {
				/* Bad luck, reuse the index for the next file. */
				return index;
			}

			/* Looks okay, generate a name for the recovered file. */
			snprintf(fname, sizeof(fname), "%s%05d.jpg", prefix, index);

			/* Open the file. */
			out = fopen(fname, "wb");
			if (!out) {
				perror(fname);
				die("Could not open target file.");
			}

			/* Write the SOI marker. */
			fputc(0xFF, out); fputc(0xD8, out);

			/* Reset the flag. */
			firstMarker = 0;
		}

		/* Copy the marker in the output stream. */
		fputc(0xFF, out);
		fputc(marker, out);

		/* Get the segment length, taking care of EOFs. */
		int length_hi = fgetc(f);
		if (length_hi < 0 || length_hi > 255) break;
		int length_lo = fgetc(f);
		if (length_lo < 0 || length_lo > 255) break;

		int length = (length_hi << 8) | length_lo;

		/* Copy the length in the output stream. */
		fputc(length_hi, out);
		fputc(length_lo, out);

		/* Copy the segment body. */
		uchar buf[65535]; /* 16-bit length, this must suffice. */
		fread(buf, length-2, 1, f);
		fwrite(buf, length-2, 1, out);

		/* Announce the segment (length + 2-byte marker). */
		printf("  * segment %02X, length %d\n", marker, length+2);

		/* If the marker is SOS = Start of Scanlines, dump'em. */
		if (marker == 0xDA) {
			/* Announce. */
			printf("  * Scanlines, dumping... ");
			fflush(stdout);

			/* Last two bytes dumped, big-endian. */
			uint16_t state = 0;
			/* The number of bytes dumped so far. */
			int count = 0;

			/* Repeat until the EOI (end-of-image) marker. */
			while (state != 0xFFD9) {
				/* Copy a byte and add it to the state, increase count. */
				uchar byte = fgetc(f);
				fputc(byte, out);
				state = (state << 8) | byte;
				++count;

				/* Check count. */
				if (count > MAX_SCANLINES_SIZE) {
					/* Too many bytes, cancel dumping the file. */
					printf("\n-> Refusing to dump more than %d kB.\n", MAX_SCANLINES_SIZE/1024);
					fclose(out);
					/* Reuse the index for the next file. */
					return index;
				}
			}
			/* Report how many bytes have been copied. */
			printf("%d bytes.\n", count);

			/* File complete, wheeee! */
			printf("-> saved successfully as %s.\n", fname);
			fclose(out);

			/* The next file will use the next index. */
			return index+1;
		}
	}

	/* Premature EOF. */
	if (out) fclose(out);
	printf("-> premature EOF.\n");

	/* Reuse the index for the next file. */
	return index;
}
