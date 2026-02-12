#include "test.h"

#ifdef __USE_GLIBC__
#include <unistd.h>
#include <sys/syscall.h>
#endif

#include "libtest.h"

__attribute__((__constructor__))
void cc(void) {
    syscall(600);
}

__attribute__((__destructor__))
void dc(void) {
    syscall(602);
}


// HACK:  Add our own _start and main
// If we build this as a shared lib, gcc won't
// link in libc, making analysis fast
__attribute__((noinline))
int main(void)
{
    syscall(601);
}

typedef int (*main_ptr)(void);

__attribute__((noinline))
void _start(void)
{
   volatile main_ptr m = &main;
   m();
}
