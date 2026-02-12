all: file

file: 
	cp /usr/bin/file .

clean:
	rm -r .sysfilter file file_syscall_list.json

test: file
	./file --help

