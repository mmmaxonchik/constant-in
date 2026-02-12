#include <unistd.h>
#include <sys/syscall.h>
#include <sys/utsname.h>
#include <time.h>
#include <sched.h>

class BranchTarget {
public:
    __attribute__((noinline))
    static void run() {
        (void)::syscall(SYS_gettid);
        (void)::syscall(SYS_getuid);
        (void)::syscall(SYS_getgid);
        (void)::syscall(SYS_umask, 022);
        (void)::syscall(SYS_sched_yield);
    }
};

class MainFlow {
public:
    static int run() {
        utsname u{};
        timespec ts{};
        volatile int never = 0;

        (void)::getpid();
        (void)::getppid();
        (void)::uname(&u);
        ts.tv_sec = 0;
        ts.tv_nsec = 13 * 1000 * 1000;
        (void)::nanosleep(&ts, nullptr);
        (void)::sched_yield();

        if (never) BranchTarget::run();
        return 0;
    }
};

int main() {
    return MainFlow::run();
}
