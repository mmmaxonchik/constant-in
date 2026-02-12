#include <unistd.h>
#include <sys/mman.h>
#include <sys/syscall.h>

static int memfd_create_wrap(const char *name, unsigned int flags) {
    return (int)syscall(SYS_memfd_create, name, flags);
}

int main(void) {
    int fd = memfd_create_wrap("x", 0);
    (void)syscall(SYS_ftruncate, fd, 4096);

    void *p = (void*)syscall(SYS_mmap, 0, 4096, 3, 2, fd, 0);
    (void)syscall(SYS_mprotect, p, 4096, 1);
    (void)syscall(SYS_munmap, p, 4096);
    (void)syscall(SYS_close, fd);
    return 0;
}
