// fcg1.c

#include <unistd.h>
#include <sys/syscall.h>

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

void f6(void);
void f7(void);

volatile fptr fp;
fptr fp_arr[] = {&f6, &f7};

void f10(void) {
    syscall(610);
}

__attribute__((constructor)) void f9 (void) {
    syscall(609);
    f10();
}

__attribute__((noinline))
void f8(void)
{
    syscall(608);
}

__attribute__((noinline))
void f7(void) {
    syscall(607);
    f8();
}

__attribute__((noinline))
void f6(void) {
    syscall(606);
}

__attribute__((noinline))
void f5(void) {
    syscall(605);
    fp_arr[1]();
}

__attribute__((noinline))
void f4(void) {
    syscall(604);
    f5();
}

__attribute__((noinline))
void f3(void) {
    syscall(603);
}

__attribute__((noinline))
fptr f2(void) {
    syscall(602);
    return &f4;
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
