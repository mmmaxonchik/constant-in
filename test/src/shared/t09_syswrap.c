#include <unistd.h>
#include <sys/syscall.h>

long t09_getpid(void) { return syscall(SYS_getpid); }
long t09_getppid(void) { return syscall(SYS_getppid); }
long t09_gettid(void) { return syscall(SYS_gettid); }
long t09_sched_yield(void) { return syscall(SYS_sched_yield); }
