
CC ?= gcc
CFLAGS ?= -Wall -g -std=c99

main: pcimem

clean:
	-rm -f *.o *~ core pcimem

