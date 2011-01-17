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
#include <string.h>

typedef int bool_t;
#define false 0
#define true 1

/** Maximum size of the SOS-EOI block, in bytes. */
#define MAX_SCANLINES_SIZE 8 * 1024 * 1024

/** An unsigned byte, to save typing. */
typedef uint8_t uchar;

/** Print a fatal error and exit. */
void die(char * msg)
{
	fprintf(stderr, "Error: %s\nAborting.\n", msg);
	exit(1);
}

/**
 * Try to recover a CR2 file from the current position in the stream.
 * @param f The input stream.
 * @param index The index used to generate the output file name.
 * @param bigEndian true iff the TIFF file is big-endian.
 * @return The next index if successful, the same index if unsuccessful.
 * @note CR2 files are structurally TIFF files.
 */
int recoverCr2(FILE * f, int index, bool_t bigEndian, const char * prefix)
{
	die("CR2 recovery not implemented yet.");
}

/**
 * Try to recover a JPEG file from the current position in the stream.
 * @param f The input stream
 * @param index The index used to generate the output file name.
 * @return The next index if successful, the same index if unsuccessful.
 */
int recoverJpeg(FILE * f, int index, const char * prefix)
{
	/** Are we processing the first marker? */
	int firstMarker = 1;

	/** The output file (recoveredXXXXX.jpg). */
	FILE * out = NULL;

	/** The name of the output file. */
	char fname[256];

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
			if (marker != 0xE0 && marker != 0xE1) {
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
 * Recover image files from the given stream.
 * @param f The stream.
 */
void recoverImages(FILE * f, bool_t jpeg, bool_t cr2, const char * prefix)
{
	/** Last two bytes read, big-endian. */
	uint16_t state = 0;

	/** The index of the next recovered file. */
	int index = 0;

	/* While not EOF... */
	while (!feof(f)) {
		/* Update the state... */
		state = (state << 8) | fgetc(f);

		/* Compare with known startcodes. */
		switch (state)
		{
			case 0xFFD8: /* JPEG Start-Of-Image */
				if (jpeg)
					index = recoverJpeg(f, index, prefix);
				break;

			case 0x4949:
			case 0x4D4D:
				if (cr2)
					index = recoverCr2(f, index, state == 0x4D4D, prefix);
				break;
		}
	}

	/* A report for the user to make them sure. */
	printf("End of image reached, quitting.\n");
}

void usage() {
	static const char * const content =
		"usage:\n"
		"    ./recover [-J] [-r] [-p <prefix>] /dev/memory_card\n"
		"\n"
		"Available options:\n"
		"    -J          -- Do not recover JPEG files.\n"
		"    -r          -- Do recover CR2 files.\n"
		"    -p <prefix> -- Use this prefix for recovered files. May contain slashes.\n"
		"                   (default: recovered)\n"
		"\n"
		"By default, the program will recover JPEG files and not CR2 files.\n"
		;
	fprintf(stderr, "%s", content);
}

int main(int argc, char ** argv)
{
	if (argc > 1) {
		/* Default options. */
		bool_t jpeg = true;
		bool_t cr2 = false;
		const char * prefix = "recovered";
		
		/* We have at least one argument, parse the commandline options. */
		char ** argEnd = argv + argc;
		char ** curArg = argv + 1;
		while (curArg < argEnd) {
			if (!strcmp("-J", *curArg))
				jpeg = false;
			else if (!strcmp("-r", *curArg))
				cr2 = true;
			else if (!strcmp("-p", *curArg)) {
				if (++curArg >= argEnd)
					die("-p requires an argument: the prefix.");

				prefix = *curArg;
			}
			else break;

			++curArg;
		}

		/* Some sanity checks. */
		if (!jpeg && !cr2)
			die("Both JPEG and CR2 recovery disabled, nothing to do.");
		if (curArg >= argEnd)
			die("Missing the last argument: /dev/memory_card or image file.");

		/* Try to open the file. */
		FILE * f = fopen(*curArg, "rb");
		if (!f) {
			/* Bad luck opening the file. */
			perror(*curArg);
			return 1;
		}

		/* Recover the images. */
		printf("Recovering images from %s...\n", *curArg);
		recoverImages(f, jpeg, cr2, prefix);

		/* Close the input file. */
		fclose(f);
	} else {
		/* Print usage info. */
		usage();
		return 1;
	}
	return 0;
}
