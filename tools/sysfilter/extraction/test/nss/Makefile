include ../env.mk

CC=gcc
CFLAGS=-O2 -Wall -g
LDFLAGS=-ldl
LDFLAGS_LOCAL=-Wl,-rpath='$$ORIGIN' -L. $(LDFLAGS)

BINS=nsstest getaddrinfo netgroup getaliasbyname getgrent

.PHONY: all clean test

all: $(BINS)

nsstest: nsstest.c
	$(CC) $(CFLAGS) -o $@ $<

getaddrinfo: getaddrinfo.c
	$(CC) $(CFLAGS) -o $@ $<

netgroup: netgroup.c
	$(CC) $(CFLAGS) -o $@ $<

getaliasbyname: getaliasbyname.c
	$(CC) $(CFLAGS) -o $@ $<

test: all
	$(PYTHON) -m unittest

clean:
	rm -rfv *.o $(PYTHON_OBJS)
	rm -fv $(SYSFILTER_ARTIFACTS)
	rm -fv $(BINS)
