// cmlib.c

#define raw_syscall(nr) {			\
    long __res; \
    asm volatile ("syscall;" \
	:"=a"(__res)					\
	:"0"(nr)					\
	:);						\
}

__attribute__((noinline))
void do_raw_add(int argc)
{
    int nr = (argc > 0) ? 602 : 603;
    raw_syscall(nr);
}


__attribute__((noinline))
void do_raw(int argc)
{
    int nr = (argc > 0) ? 742 : 655;
    raw_syscall(nr);
}
