include ../env.mk

CC=gcc
CFLAGS_MIN=-Wall

.PHONY: all clean test

all: raw raw-at

raw: raw.c
	# Disable optimizations to prevent inlining
	$(CC) $(CFLAGS_MIN) -O0 -g $< -o $@

raw-at: raw-at.c
	$(CC) $(CFLAGS_MIN) -O2 -g $< -o $@

test: all
	$(PYTHON) -m unittest

clean: cleanobj
	$(RM) -fv *.o
	$(RM) -fv raw raw-at
