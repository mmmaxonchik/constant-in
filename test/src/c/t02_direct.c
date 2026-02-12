#include <stdint.h>
#include <sys/syscall.h>

struct utsname_ {
    char sysname[65], nodename[65], release[65], version[65], machine[65], domainname[65];
};

static inline long sc0(long n) {
    long ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(n) : "rcx","r11","memory");
    return ret;
}
static inline long sc1(long n, long a1) {
    long ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(n), "D"(a1) : "rcx","r11","memory");
    return ret;
}

int main(void) {
    struct utsname_ u;

    (void)sc0(SYS_getpid);
    (void)sc0(SYS_getppid);
    (void)sc0(SYS_gettid);
    (void)sc1(SYS_uname, (long)&u);
    (void)sc0(SYS_sched_yield);

    return 0;
}
