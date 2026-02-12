#include <unistd.h>
#include <sys/utsname.h>
#include <time.h>
#include <sched.h>

class ApiMainFlow {
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
    return ApiMainFlow::run();
}
