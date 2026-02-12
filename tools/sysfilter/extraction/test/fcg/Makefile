include ../env.mk

CC=gcc
STRIP=strip
CFLAGS_MIN=-O2 -Wall
CFLAGS=$(CFLAGS_MIN) -g
LDFLAGS=-ldl
LDFLAGS_LOCAL=-Wl,-rpath='$$ORIGIN' -L. $(LDFLAGS)

BINS=fcg1 fcg1-stripped fcg2

.PHONY: all clean test

all: $(BINS)

fcg1: fcg1.c
	$(CC)  $(CFLAGS) -o $@ $<

fcg1-stripped: fcg1.c
	$(CC) $(CFLAGS_MIN) -o $@ $<
	$(STRIP) $@

fcg2: fcg2.c
	$(CC)  $(CFLAGS) -o $@ $<

test: all
	$(PYTHON) -m unittest

clean:
	rm -rfv *.o $(PYTHON_OBJS)
	rm -fv $(SYSFILTER_ARTIFACTS)
	rm -fv $(BINS)
