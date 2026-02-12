#!/bin/bash

if [[ $1 == "" ]]; then
    echo "usage: $0 output-file"
    exit 1
fi

tmpdir=`mktemp -d`

gcc -o ${tmpdir}/extract -xc - <<EOF
#include <stdio.h>
#include <sys/auxv.h>
#include <elf.h>
#include <unistd.h>
#include <fcntl.h>

int main(int argc, char *argv[]) {
    int fd = open(argv[1], O_WRONLY | O_CREAT, 0644);
    if(fd == -1) {
        perror("open");
        return 1;
    }
    // always at most one page
    Elf64_Ehdr *ehdr = (void *)getauxval(AT_SYSINFO_EHDR);

    Elf64_Off maxOff = 0;
    Elf64_Phdr *pheaders = (void *)((char *)ehdr + ehdr->e_phoff);

    for(Elf64_Half index = 0; index < ehdr->e_phnum; index ++) {
        Elf64_Off last = pheaders[index].p_offset + pheaders[index].p_filesz;
        if(last > maxOff) maxOff = last;
    }

    Elf64_Off headlast = ehdr->e_shoff + (ehdr->e_shentsize * ehdr->e_shnum);
    if(headlast > maxOff) maxOff = headlast;
    headlast = ehdr->e_phoff + (ehdr->e_phentsize * ehdr->e_phnum);
    if(headlast > maxOff) maxOff = headlast;

    printf("Size: %ld\n", maxOff);

    write(fd, ehdr, maxOff);

    close(fd);
    return 0;
}
EOF

${tmpdir}/extract $1

rm -rf ${tmpdir}
