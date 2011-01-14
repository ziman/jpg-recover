/*
 * recover.c
 * Extract JPEG files from raw bytes (filesystem images etc.).
 *
 * Copyright (c) 2011, Matus Tejiscak <functor.sk@ziman>
 *
 * Released under the BSD license.
 * http://creativecommons.org/licenses/BSD/
 *
 * Purpose:
 *	This program can be used to extract accidentally deleted JPEG images
 *	from any memory media (filesystem images, memory cards, etc).
 *
 * Usage:
 *	$ cc -O2 recover.c -o recover
 *	$ ./recover /dev/memory_card
 *	...and just watch JPEG files appear in the CWD.
 *
 * Warning:
 *	The program uses a fixed naming scheme (recovered%05d.jpg) and will
 *	happily overwrite any files with coinciding names in the CWD without
 *	warning.
 *
 * A benchmark:
 *	I dumped 300 images from a 1G CF card in:
 *		real	1m35.379s
 *		user	0m39.362s
 * 		sys	0m5.468s
 *	The processor was 50% idle, which means the process was IO-bound.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

/** Maximum size of the SOS-EOI block, in bytes. */
#define MAX_SCANLINES_SIZE 8 * 1024 * 1024

/** An unsigned byte, to save typing. */
typedef uint8_t uchar;

/** Print a fatal error and exit. */
void die(char * msg)
{
	fprintf(stderr, "%s\n", msg);
	exit(1);
}

/**
 * Try to recover a JPEG file from the current position in the stream.
 * @param f The input stream
 * @param index The index used to generate the output file name.
 * @return The next index if successful, the same index if unsuccessful.
 */
int tryToRecover(FILE * f, int index)
{
	/** Are we processing the first marker? */
	int firstMarker = 1;

	/** The output file (recoveredXXXXX.jpg). */
	FILE * out = NULL;

	/** The name of the output file. */
	char fname[256];

	/* In this position, we have already read FF D8. */

	while (!feof(f))
	{
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
		if (firstMarker)
		{
			/* First marker, an E0 or E1 marker must follow. */
			if (marker != 0xE0 && marker != 0xE1)
			{
				/* Bad luck, reuse the index for the next file. */
				return index;
			}

			/* Looks okay, generate a name for the recovered file. */
			snprintf(fname, sizeof(fname), "recovered%05d.jpg", index);

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

		/* Get the segment length. */
		uchar length_hi = fgetc(f);
		uchar length_lo = fgetc(f);
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
			printf("%d bytes.\n", count, state);

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

/**
 * Recover JPEG files from the given stream.
 * @param f The stream.
 */
void recoverJpegs(FILE * f)
{
	/** Last two bytes read, big-endian. */
	uint16_t state = 0;

	/** The index of the next recovered file. */
	int index = 0;

	/* While not EOF... */
	while (!feof(f))
	{
		/* Update the state... */
		state = (state << 8) | fgetc(f);

		/* If SOI found, proceed. */
		if (state == 0xFFD8)
			index = tryToRecover(f, index);
	}
}

int main(int argc, char * argv[])
{
	if (argc > 1)
	{
		/* We have at least one argument, try to open the input file. */
		FILE * f = fopen(argv[1], "rb");
		if (!f)
		{
			/* Bad luck opening the file. */
			perror(argv[1]);
			return 1;
		}

		/* Recover'em and close the input file. */
		printf("Recovering images from %s...\n", argv[1]);
		recoverJpegs(f);
		fclose(f);
	}
	else
	{
		/* Print usage info. */
		fprintf(stderr, "usage: ./recover /dev/memory_card\n");
	}
	return 0;
}
