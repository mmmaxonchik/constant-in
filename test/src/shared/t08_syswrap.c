#include <unistd.h>
#include <sys/syscall.h>

long t08_getpid(void) { return syscall(SYS_getpid); }
long t08_getppid(void) { return syscall(SYS_getppid); }
long t08_gettid(void) { return syscall(SYS_gettid); }
long t08_sched_yield(void) { return syscall(SYS_sched_yield); }
