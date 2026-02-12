.PHONY: all regen deps test clean reformat
all: deps
	$(MAKE) -C app
deps:
	$(MAKE) -C egalito src
test: all
	$(MAKE) -C test test
clean:
	$(MAKE) -C app clean
reformat:
	$(MAKE) -C app reformat
