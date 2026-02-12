#define _GNU_SOURCE

#include <dlfcn.h>
#include <unistd.h>
#include <sys/syscall.h>


__attribute__((__constructor__))
void cc(void) {
    syscall(600);
    dlsym(RTLD_DEFAULT, "printf");
}

__attribute__((__destructor__))
void dc(void) {
    syscall(602);
    dlsym(RTLD_DEFAULT, "shutdown");
}


int main(void)
{
    dlsym(RTLD_DEFAULT, "time");
    syscall(601);
}
