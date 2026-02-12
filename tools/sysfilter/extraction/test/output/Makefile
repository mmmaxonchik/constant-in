include ../env.mk

CC=gcc
STRIP=strip
CFLAGS=-O2 -Wall
LDFLAGS=
LDFLAGS_LOCAL=-Wl,-rpath='$$ORIGIN' -L. $(LDFLAGS)

.PHONY: all clean test

all: liba.so liba-stripped.so hello hello-stripped hello-libstripped

liba.so: liba.c liba.h
	$(CC) $(CFLAGS) -fPIC -shared -o $@ $<

liba-stripped.so: liba.so
	$(STRIP) --strip-all -o $@ $<

hello: hello.c
	$(CC) $(CFLAGS) -g -o $@ $< $(LDFLAGS_LOCAL) -la

hello-stripped: hello
	$(STRIP) --strip-all -o $@ $<


hello-libstripped: hello.c liba-stripped.so
	$(CC) $(CFLAGS) -g -o $@ $< $(LDFLAGS_LOCAL) -la-stripped

test: all
	$(PYTHON) -m unittest

clean:
	rm -fv *.o
	rm -fv hello hello-stripped hello-libstripped liba.so liba-stripped.so
