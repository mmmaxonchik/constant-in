#include <unistd.h>
#include <sys/syscall.h>
#include <sys/utsname.h>
#include <time.h>
#include <sched.h>

__attribute__((noinline))
static void branch_target(void) {
    (void)syscall(SYS_gettid);
    (void)syscall(SYS_getuid);
    (void)syscall(SYS_getgid);
    (void)syscall(SYS_umask, 022);
    (void)syscall(SYS_sched_yield);
}

int main(void) {
    struct utsname u;
    struct timespec ts;

    volatile int never = 0;

    (void)getpid();
    (void)getppid();
    (void)uname(&u);

    ts.tv_sec = 0;
    ts.tv_nsec = 13 * 1000 * 1000;
    (void)nanosleep(&ts, 0);

    (void)sched_yield();

    if (never) branch_target();
    return 0;
}
