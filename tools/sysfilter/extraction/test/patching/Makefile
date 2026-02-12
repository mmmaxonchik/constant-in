CC=gcc
CFLAGS=-O2 -Wall -g
LDFLAGS=
#LDFLAGS_LOCAL=-Wl,-rpath='$$ORIGIN' -L. $(LDFLAGS)
LDFLAGS_LOCAL=-L. $(LDFLAGS)

ifeq ($(DEBUG),1)
        CFLAGS += -DDEBUG
endif

.PHONY: all patch clean

all: libfoo.so libbar.so libfoohack.so foo

libfoo.so: libfoo.c
	$(CC) $(CFLAGS) -fPIC -shared -o $@ $<

libbar.so: libbar.c libfoo.so
	$(CC) $(CFLAGS) -fPIC -shared -o $@ $< $(LDFLAGS_LOCAL) -lfoo

libfoohack.so: libfoohack.c
	$(CC) $(CFLAGS) -fPIC -shared -o $@ $< -ldl

foo: foo.c foo.h libfoo.so libbar.so
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS_LOCAL) -lfoo -lbar

patch-lib: libfoo.so libfoohack.so
	patchelf --add-needed $(word 2,$^) $<

patch-bin: libfoohack.so foo
	patchelf --add-needed libfoohack.so foo

clean:
	rm -fv *.o libfoo.so libfoohack.so libbar.so foo
