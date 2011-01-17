CFLAGS = -D_FILE_OFFSET_BITS=64 -W -Wall

%.o: %.c
	cc $(CFLAGS) -O2 -c -o $@ $<

recover: main.o jpeg.o tiff.o globals.o
	cc -O2 -o recover main.o jpeg.o tiff.o globals.o

win: *.c
	i586-mingw32msvc-cc $(CFLAGS) -O2 -o recover.exe *.c

clean:
	-rm -f *.o recover *~ *.exe
