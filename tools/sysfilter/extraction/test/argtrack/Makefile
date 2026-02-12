include ../env.mk

# Remove conditional moves:   -fno-if-conversion -fno-if-conversion2
CC=gcc
CFLAGS=-O2 -Wall -g
LDFLAGS=-ldl
LDFLAGS_LOCAL=-Wl,-rpath='$$ORIGIN' -L. $(LDFLAGS)

BINS=libargtrack.so libat2.so at0 argtrack1 argtrack2 at3 at4 at6 at7

.PHONY: all clean test

all: $(BINS)

libargtrack.so: libargtrack.c libargtrack.h
	$(CC) $(CFLAGS) -fPIC -shared -o $@ $< -ldl

libat2.so: libat2.c libargtrack.h
	$(CC) $(CFLAGS) -fPIC -shared -o $@ $< -ldl

argtrack1: argtrack1.c
	$(CC)  $(CFLAGS) -o $@ $< $(LDFLAGS_LOCAL) -largtrack

argtrack2: argtrack2.c
	$(CC)  $(CFLAGS) -o $@ $< $(LDFLAGS_LOCAL) -lat2

at0: at0.c
	$(CC)  $(CFLAGS) -o $@ $< $(LDFLAGS_LOCAL)

at3: at3.c
	$(CC)  $(CFLAGS) -o $@ $< $(LDFLAGS_LOCAL) -lat2

at4: at4.c
	$(CC)  $(CFLAGS) -o $@ $< $(LDFLAGS_LOCAL)

at6: at6.c
	$(CC)  $(CFLAGS) -o $@ $< $(LDFLAGS_LOCAL) -lpthread

at7: at7.c
	$(CC)  $(CFLAGS) -o $@ $<

test: all
	$(PYTHON) -m unittest

clean:
	rm -rfv *.o $(PYTHON_OBJS)
	rm -fv $(SYSFILTER_ARTIFACTS)
	rm -fv $(BINS)
