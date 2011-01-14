recover: recover.c
	cc -O2 -o recover recover.c

clean:
	-rm -f *.o recover *~