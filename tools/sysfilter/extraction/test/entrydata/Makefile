include ../env.mk


CC=gcc
CFLAGS=-O0 -Wall -g -fPIC
LDFLAGS=
LDFLAGS_LOCAL=-Wl,-rpath='$$ORIGIN' -L. $(LDFLAGS)

.PHONY: all clean test

all: liba.so libb.so libd.so entrydatatest

liba.so: liba.c liba.h
	$(CC) $(CFLAGS) -shared -o $@ $<

libb.so: libb.c libb.h
	$(CC) $(CFLAGS) -shared -o $@ $< $(LDFLAGS_LOCAL) -la

libd.so: libd.c libd.h
	$(CC) $(CFLAGS) -shared -o $@ $< $(LDFLAGS_LOCAL) -la

entrydatatest: entrydatatest.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS_LOCAL) -la -lb

test: all
	$(PYTHON) -m unittest

clean:
	rm -fv *.o liba.so libb.so libd.so
	rm -fv entrydatatest
