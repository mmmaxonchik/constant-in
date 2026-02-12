include ../env.mk

CC=gcc
CFLAGS=-Wall -O2 -g
LDFLAGS=
LDFLAGS_LOCAL=-Wl,-rpath='$$ORIGIN' -L. $(LDFLAGS)

.PHONY: all clean test

BINS=true libdata.so libextra.so usedata

all: $(BINS)

true: true.c
	$(CC)  $(CFLAGS) $< -o $@

libdata.so: libdata.c
	$(CC)  $(CFLAGS) -fPIC -shared $< -o $@

libextra.so: libextra.c
	$(CC)  $(CFLAGS) -fPIC -shared $< -o $@

usedata: usedata.c libdata.so
	$(CC)  $(CFLAGS) $< -o $@ $(LDFLAGS_LOCAL) -ldata

test: all
	$(PYTHON) -m unittest

clean:
	rm -rfv *.o $(PYTHON_OBJS)
	rm -fv $(SYSFILTER_ARTIFACTS)
	rm -fv $(BINS)
