CC = gcc
CFLAGS = -g -O0 -Wall -Werror -MMD
LDLIBS = -lev

all: mersenne

dlm: mersenne.o

clean:
	rm -f *.o
	rm -f mersenne

.PHONY: all clean
