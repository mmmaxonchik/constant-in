include ../env.mk


CC=gcc
CFLAGS=-O0 -Wall -g -fPIC
LDFLAGS=
LDFLAGS_LOCAL=-Wl,-rpath='$$ORIGIN' -L. $(LDFLAGS)

.PHONY: all clean test

all: libtest.so dltest

libtest.so: libtest.c libtest.h
	$(CC) $(CFLAGS) -shared -o $@ $< $(LDFLAGS_LOCAL)

dltest: dltest.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS_LOCAL) -ldl

test: all
	$(PYTHON) -m unittest

clean:
	rm -fv *.o libtest.so dltest
