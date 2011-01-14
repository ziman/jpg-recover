recover: main.c
	cc -O2 -o recover main.c

clean:
	-rm -f *.o recover *~