
all: empty

empty: empty.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $< -o $@

clean:
	rm -r .sysfilter empty empty_syscall_list.json

test: empty
	./empty
