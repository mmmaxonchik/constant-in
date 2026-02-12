// cmov1.c
#include <unistd.h>
#include <sys/syscall.h>

#define raw_syscall(nr) {			\
    long __res; \
    asm volatile ("syscall;" \
	:"=a"(__res)					\
	:"0"(nr)					\
	:);						\
}

__attribute__((noinline))
void do_func(int argc)
{
    int nr = (argc > 0) ? 600 : 601;
    syscall(nr);
}

__attribute__((noinline))
void do_raw(int argc)
{
    int nr = (argc > 0) ? 602 : 603;
    raw_syscall(nr);
}

int main(int argc, char **argv)
{
    do_func(argc);
    do_raw(argc);

    return 0;
}
