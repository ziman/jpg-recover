#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define MAX_SCANLINES_SIZE 16 * 1024 * 1024

typedef uint8_t uchar;
typedef uint16_t bichar;

void die(char * msg)
{
	printf("%s\n", msg);
	exit(1);
}

int tryToRecover(FILE * f, int index)
{
	int firstMarker = 1;
	int sosCount = 0;
	FILE * out = NULL;
	char fname[256];
	
	while (!feof(f))
	{
		uchar ff = fgetc(f);
		if (feof(f)) {
			if (out) fclose(out);
			return index+1;
		}
		
		if (ff != 0xFF) {
			if (!firstMarker)
				printf("-> quitting on invalid marker.\n");
			if (out) fclose(out);
			return index;
		}

		uchar marker = fgetc(f);
		if (firstMarker)
		{
			if (marker != 0xE0 && marker != 0xE1)
			{
				// printf("-> quitting on missing exif marker.\n");
				return index;
			}

			snprintf(fname, sizeof(fname), "recovered%05d.jpg", index);
			out = fopen(fname, "wb");
			if (!out) {
				perror(fname);
				die("Could not open target file.");
			}
			fputc(0xFF, out); fputc(0xD8, out);
			firstMarker = 0;
		}

		fputc(0xFF, out);
		fputc(marker, out);

		uchar length_hi = fgetc(f);
		uchar length_lo = fgetc(f);
		int length = (length_hi << 8) | length_lo;

		fputc(length_hi, out);
		fputc(length_lo, out);

		uchar buf[65535];
		fread(buf, length-2, 1, f);
		fwrite(buf, length-2, 1, out);

		printf("  * marker %02X, length %d\n", marker, length);

		if (marker == 0xDA) {
			printf("  * Scanlines, dumping... ");
			uint16_t state = 0;
			int count = 0;
			while (state != 0xFFD9) {
				uchar byte = fgetc(f);
				fputc(byte, out);
				state = (state << 8) | byte;
				++count;
				if (count > MAX_SCANLINES_SIZE) {
					printf("\n-> Refusing to dump more than %d kB.\n", MAX_SCANLINES_SIZE/1024);
				}
			}
			printf("%d bytes.\n", count, state);
			
			if (1)
			{
				printf("-> saved successfully as %s.\n", fname);
				fclose(out);
				return index+1;
			}
		}
	}
	if (out) fclose(out);

	printf("-> premature EOF.\n");
	return index;
}

void findRecoverJpeg(FILE * f)
{
	uint16_t state = 0;
	int index = 0;
	
	while (!feof(f))
	{
		state = (state << 8) | fgetc(f);
		if (state == 0xFFD8)
			index = tryToRecover(f, index);
	}
}

int main(int argc, char * argv[])
{
	if (argc > 1)
	{
		FILE * f = fopen(argv[1], "rb");
		if (!f)
		{
			perror(argv[1]);
			return 1;
		}

		printf("Recovering images from %s...\n", argv[1]);
		
		findRecoverJpeg(f);

		fclose(f);
	}
	else
	{
		fprintf(stderr, "usage: ./parse <image.jpg>\n");
	}
	return 0;
}