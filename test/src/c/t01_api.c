#include <unistd.h>
#include <sys/utsname.h>
#include <time.h>
#include <sched.h>

int main(void) {
    struct utsname u;
    struct timespec ts;

    (void)getpid();
    (void)getppid();
    (void)uname(&u);

    ts.tv_sec = 0;
    ts.tv_nsec = 13 * 1000 * 1000;
    (void)nanosleep(&ts, 0);

    (void)sched_yield();
    return 0;
}
