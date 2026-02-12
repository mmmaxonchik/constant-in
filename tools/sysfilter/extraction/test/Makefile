include env.mk

TEST_DIRS = \
	hello \
	raw \
	output \
	entrydata \
	argtrack \
	partition \
	args \
	dl \
	nss \
	threadpart \
	fcg \
	cmov

.PHONY: test clean
all:
	for dir in $(TEST_DIRS); do \
		make -C $$dir all; \
	done

test: all
	$(PYTHON) -m unittest discover

clean:
	for dir in $(TEST_DIRS); do \
		make -C $$dir clean; \
	done

cleanobj:
	for dir in $(TEST_DIRS); do \
		make -C $$dir cleanobj; \
	done

testclean:
	for dir in $(TEST_DIRS); do \
		make -C $$dir testclean; \
	done
