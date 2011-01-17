%.o: %.c
	cc -O2 -c -o $@ $<

recover: main.o jpeg.o tiff.o globals.o
	cc -O2 -o recover main.o jpeg.o tiff.o globals.o

clean:
	-rm -f *.o recover *~