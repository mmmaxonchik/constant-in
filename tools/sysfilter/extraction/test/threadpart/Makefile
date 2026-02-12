include ../env.mk

CC=gcc
CFLAGS=-O2 -Wall -g
LDFLAGS=-ldl
LDFLAGS_LOCAL=-Wl,-rpath='$$ORIGIN' -L. $(LDFLAGS)

BINS=part-pthread part-libpth part-wrap part-clone libpth.so ex

.PHONY: all clean test

all: $(BINS)

# libargtrack.so: libargtrack.c libargtrack.h
# 	$(CC) $(CFLAGS) -fPIC -shared -o $@ $< -ldl
libpth.so: libpth.c
	$(CC) $(CFLAGS) -fPIC -shared -o $@ $< -lpthread

ex: ex.c
	$(CC)  $(CFLAGS) -o $@ $< -ldl

part-pthread: part-pthread.c libpth.so
	$(CC)  $(CFLAGS) -o $@ $< $(LDFLAGS_LOCAL) -lpth -lpthread

part-libpth: part-libpth.c libpth.so
	$(CC)  $(CFLAGS) -o $@ $< $(LDFLAGS_LOCAL) -lpth -lpthread

part-wrap: part-wrap.c libpth.so
	$(CC)  $(CFLAGS) -o $@ $< $(LDFLAGS_LOCAL) -lpth -lpthread

part-clone: part-clone.c libpth.so
	$(CC)  $(CFLAGS) -o $@ $<

test: all
	$(PYTHON) -m unittest

clean:
	rm -rfv *.o $(PYTHON_OBJS)
	rm -fv $(SYSFILTER_ARTIFACTS)
	rm -fv $(BINS)
