include ../env.mk

# Remove conditional moves:   -fno-if-conversion -fno-if-conversion2
CC=gcc
CFLAGS=-O2 -Wall -g
CFLAGS_NOCMOV=$(CFLAGS) -fno-if-conversion -fno-if-conversion2
LDFLAGS=-ldl
LDFLAGS_LOCAL=-Wl,-rpath='$$ORIGIN' -L. $(LDFLAGS)

BINS=cmov1 cmov1-noif cmlib.so

.PHONY: all clean test

all: $(BINS)

cmov1: cmov1.c
	$(CC)  $(CFLAGS) -o $@ $<

cmov1-noif: cmov1.c
	$(CC)  $(CFLAGS_NOCMOV) -o $@ $<

cmlib.so: cmlib.c
	$(CC) $(CFLAGS) -shared -o $@ $<

test: all
	$(PYTHON) -m unittest

clean:
	rm -rfv *.o $(PYTHON_OBJS)
	rm -fv $(SYSFILTER_ARTIFACTS)
	rm -fv $(BINS)
