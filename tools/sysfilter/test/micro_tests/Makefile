# This makefile is currently not used by test.py

TEST_DIRS = \
	empty \
	file \
	grep \
	hello \

.PHONY: test clean

all:
	for dir in $(TEST_DIRS); do \
		make -C $$dir all; \
	done

clean:
	for dir in $(TEST_DIRS); do \
		make -C $$dir clean; \
	done

