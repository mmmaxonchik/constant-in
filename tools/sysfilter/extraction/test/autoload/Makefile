CC=gcc
CFLAGS=-O0 -Wall -g
LDFLAGS=-ldl
LDFLAGS_LOCAL=-Wl,-rpath='$$ORIGIN' -L. $(LDFLAGS)

.PHONY: all clean

all: liba.so libb.so libd.so libe.so \
dltest1 dltest2 dltest3 dltest4 dltest5 dltest-local-lib dltest-local-bin dltest-pthread dltest-pthread32 dltest-pthread-musl

liba.so: liba.c liba.h
	$(CC) $(CFLAGS) -fPIC -shared -o $@ $<

libb.so: libb.c libb.h
	$(CC) $(CFLAGS) -fPIC -shared -o $@ $<

libd.so: libd.c libd.h
	$(CC) $(CFLAGS) -fPIC -shared -o $@ $<

libe.so: libe.c libe.h
	$(CC) $(CFLAGS) -fPIC -shared -ldl -o $@ $<

dltest1: dltest1.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

dltest2: dltest2.c liba.so
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS_LOCAL) -la

dltest3: dltest3.c liba.so libb.so
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS_LOCAL) -lb

dltest4: dltest4.c liba.so libb.so
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS_LOCAL) -la -lb

dltest5: dltest5.c liba.so libb.so
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS_LOCAL) -ld

dltest-local-lib: dltest-local-lib.c libe.so
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS_LOCAL) -le -ldl

dltest-local-bin: dltest-local-bin.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS_LOCAL) -ldl

dltest-pthread: dltest-pthread.c
	$(CC) $(CFLAGS) -o $@ $< $(DLFLAGS_LOCAL) -lpthread -ldl

dltest-pthread32: dltest-pthread32.c
	$(CC) $(CFLAGS) -o $@ $< $(DLFLAGS_LOCAL) -lpthread -ldl

dltest-pthread-musl: dltest-pthread-musl.c
	$(CC) $(CFLAGS) -o $@ $< $(DLFLAGS_LOCAL) -lpthread -ldl

clean:
	rm -fv *.o liba.so libb.so libd.so libe.so
	rm -fv dltest dltest2 dltest3 dltest4 dltest-local-lib dltest-pthread dltest-pthread32 dltest-pthread-musl
