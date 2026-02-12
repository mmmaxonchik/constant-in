// fcg2.c

#include <unistd.h>
#include <sys/syscall.h>

typedef void (*fptr)(void);
volatile fptr fp;

static inline int my_syscall(long nr) {
    long __res;
    asm volatile
    ("syscall"
     :"=a"(__res)
     :"a"(nr)
     : "rcx", "r11"
	);
    return 0;
}

typedef void (*fptr)(void);

__attribute__((noinline))
void f3(void) {
    syscall(603);
}

__attribute__((noinline))
fptr f1(void) {
    syscall(601);
    return &f3;
}


int main(int argc, char **argv)
{
    syscall(600);

    fp = f1();
    fp();

    return 0;
}
