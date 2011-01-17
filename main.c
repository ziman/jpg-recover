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
 * @param jpeg True to run JPEG recovery.
 * @param cr2 True to run CR2 recovery.
 * @param prefix The prefix used to generate the names of the recovered files.
 */
void recoverImages(FILE * f, bool_t jpeg, bool_t cr2, bool_t requireE0E1, const char * prefix)
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
					index = recoverJpeg(f, index, requireE0E1, prefix);
				break;

			case 0x4949: /* TIFF endianity sign */
			case 0x4D4D:
				if (cr2) {
					/* Try to recover the TIFF file. */
					off_t beforeTiff = ftello(f);
					int newIndex = recoverTiff(f, index, state == 0x4D4D, prefix);

					/* Check the success. */
					if (newIndex == index) {
						/* Unsuccessful -> rewind to get at least the JPEG thumbnails. */
						fseeko(f, beforeTiff, SEEK_SET);
					} else {
						index = newIndex;
					}
				}
				break;
		}
	}

	/* A report for the user to make them sure. */
	printf("End of image reached, quitting.\n");
}

/** Print usage, don't quit. */
void usage() {
	static const char * const content =
		"usage:\n"
		"    ./recover [-j] [-e] [-r] [-p <prefix>] [-h/--help] /dev/memory_card\n"
		"\n"
		"Available options:\n"
		"    -j          -- Do not recover JPEG files.\n"
		"    -e          -- Recover JPEG files embedded in other files.\n"
		"                   (default: do not recover embedded JPEGs)\n"
		"    -r          -- Do not recover CR2 files.\n"
		"    -p <prefix> -- Use this prefix for recovered files. May contain slashes.\n"
		"                   (default: \"recovered\")\n"
		"    -h / --help -- Print this help and quit successfully.\n"
		"\n"
		"By default, the program will recover both JPEG files and CR2 files.\n"
		;
	fprintf(stderr, "%s", content);
}

int main(int argc, char ** argv)
{
	if (argc > 1) {
		/* Default options. */
		bool_t jpeg = true;
		bool_t cr2 = true;
		bool_t requireE0E1 = true;
		const char * prefix = "recovered";
		
		/* We have at least one argument, parse the commandline options. */
		char ** argEnd = argv + argc;
		char ** curArg = argv + 1;
		while (curArg < argEnd) {
			if (!strcmp("-J", *curArg)) {
				jpeg = false;
			}
			else if (!strcmp("-r", *curArg)) {
				cr2 = false;
			}
			else if (!strcmp("-e", *curArg)) {
				requireE0E1 = false;
			}
			else if (!strcmp("-p", *curArg)) {
				if (++curArg >= argEnd)
					die("-p requires an argument: the prefix.");

				prefix = *curArg;
			}
			else if (!strcmp("--help", *curArg) || !strcmp("-h", *curArg)) {
				usage();
				return 0;
			}
			else break;

			/* Move on... */
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
		recoverImages(f, jpeg, cr2, requireE0E1, prefix);

		/* Close the input file. */
		fclose(f);
	} else {
		/* Print usage info. */
		usage();
		return 1;
	}
	return 0;
}
