
all: hello

hello: hello.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $< -o $@

clean:
	rm -r .sysfilter hello hello_syscall_list.json

test: hello
	./hello
