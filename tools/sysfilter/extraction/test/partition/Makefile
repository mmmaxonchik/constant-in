include ../env.mk

CC=gcc
CFLAGS=-O2 -Wall -g
LDFLAGS=-ldl
LDFLAGS_LOCAL=-Wl,-rpath='$$ORIGIN' -L. $(LDFLAGS)

BINS=part part-func part-nss part-init

.PHONY: all clean test

all: $(BINS)

# libargtrack.so: libargtrack.c libargtrack.h
# 	$(CC) $(CFLAGS) -fPIC -shared -o $@ $< -ldl


part: part.c
	$(CC)  $(CFLAGS) -o $@ $<

part-func: part-func.c
	$(CC)  $(CFLAGS) -o $@ $<

part-nss: part-nss.c
	$(CC)  $(CFLAGS) -o $@ $<

part-init: part-init.c
	$(CC)  $(CFLAGS) -o $@ $<

test: all
	$(PYTHON) -m unittest

clean:
	rm -rfv *.o $(PYTHON_OBJS)
	rm -fv $(SYSFILTER_ARTIFACTS)
	rm -fv $(BINS)
