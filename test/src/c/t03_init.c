#include <unistd.h>
#include <sys/syscall.h>
#include <sys/utsname.h>
#include <sched.h>

__attribute__((constructor))
static void at_exec(void) {
    struct utsname u;

    (void)getppid();
    (void)getpid();
    (void)syscall(SYS_gettid);
    (void)uname(&u);
    (void)sched_yield();
}

int main(void) {
    return 0;
}
