include ../env.mk

CC=gcc
CFLAGS=-O0 -Wall -g
CFLAGS_MIN=-Wall
LDFLAGS=-ldl
LDFLAGS_LOCAL=

.PHONY: all clean test

all: hello hello-noopt hello-nodebug

hello: hello.c
	$(CC)  $(CFLAGS_MIN) -O2 -g $< -o $@

hello-noopt: hello.c
	$(CC) $(CFLAGS_MIN) -O0 -g $< -o $@

hello-nodebug: hello.c
	$(CC) $(CFLAGS_MIN) -O2 $< -o $@

test:
	$(PYTHON) -m unittest

clean:
	rm -rfv *.o $(PYTHON_OBJS)
	rm -fv $(SYSFILTER_ARTIFACTS)
	rm -fv hello hello-noopt hello-nodebug
