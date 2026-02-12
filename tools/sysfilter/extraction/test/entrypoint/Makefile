CC=gcc
CFLAGS=-O0 -Wall -g
LDFLAGS=
LDFLAGS_LOCAL=-Wl,-rpath='$$ORIGIN' -L. $(LDFLAGS)

.PHONY: all clean

all: liba.so libb.so entrytest1

liba.so: liba.c liba.h
	$(CC) $(CFLAGS) -fPIC -shared -o $@ $<

libb.so: libb.c libb.h
	$(CC) $(CFLAGS) -fPIC -shared -o $@ $<

entrytest1: entrytest1.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS_LOCAL) -la


clean:
	rm -fv *.o liba.so libb.so
	rm -fv entrytest1
