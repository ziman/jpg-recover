#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "globals.h"
#include "jpeg.h"
#include "tiff.h"

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
					index = recoverTiff(f, index, state == 0x4D4D, prefix);
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
		if (strlen(prefix) > MAX_PREFIX_LENGTH)
			die("Prefix too long.");

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
