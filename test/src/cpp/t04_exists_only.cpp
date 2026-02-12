#include <unistd.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <time.h>
#include <sched.h>

class ExistsOnly {
public:
    __attribute__((used, noinline))
    static void hidden() {
        (void)::getuid();
        (void)::geteuid();
        (void)::getgid();
        (void)::getegid();
        (void)::umask(022);
    }
};

class MainFlow {
public:
    static int run() {
        utsname u{};
        timespec ts{};
        (void)::getpid();
        (void)::getppid();
        (void)::uname(&u);
        ts.tv_sec = 0;
        ts.tv_nsec = 13 * 1000 * 1000;
        (void)::nanosleep(&ts, nullptr);
        (void)::sched_yield();
        return 0;
    }
};

int main() {
    return MainFlow::run();
}
