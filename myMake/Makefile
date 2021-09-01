#
# Makefile for lab 4 part 2
#

CC  = g++-8
CXX = g++-8

CFLAGS   = -g -Wall
CXXFLAGS = -g -Wall -std=c++17 -I ./part1

LDLIBS = -lstdc++fs

executable = mymake
headers = maker.h exception.h

$(executable): maker.o

# header dependency

mymake.o: $(headers)

maker.o: $(headers) helper.h

.PHONY: clean
clean:
	rm -f *.o *~ a.out core $(executable)

.PHONY: all
all: clean mymake
