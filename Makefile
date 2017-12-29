CFLAGS=--std=c99 -Wall -pedantic -g -O0

all: test
test.o: ext_args.h
example.o: ext_args.h

clean:
	rm -f *.o test example
