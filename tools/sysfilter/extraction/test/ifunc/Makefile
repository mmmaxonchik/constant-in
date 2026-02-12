include ../env.mk

CC=gcc
CFLAGS=-O2 -Wall -g
LDFLAGS=-ldl
LDFLAGS_LOCAL=-Wl,-rpath='$$ORIGIN' -L. $(LDFLAGS)

BINS=part-ifunc libifunc.so

.PHONY: all clean test

all: $(BINS)
libifunc.so: libifunc.c libifunc.h
	$(CC) $(CFLAGS) -fPIC -shared -o $@ $<

part-ifunc: part-ifunc.c libifunc.so
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS_LOCAL) -lifunc

test: all
	$(PYTHON) -m unittest

clean:
	rm -rfv *.o $(PYTHON_OBJS)
	rm -fv $(SYSFILTER_ARTIFACTS)
	rm -fv $(BINS)
