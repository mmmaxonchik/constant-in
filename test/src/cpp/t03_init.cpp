#include <unistd.h>
#include <sys/syscall.h>
#include <sys/utsname.h>
#include <sched.h>

class BeforeMain {
public:
    BeforeMain() {
        utsname u{};
        (void)::getppid();
        (void)::getpid();
        (void)::syscall(SYS_gettid);
        (void)::uname(&u);
        (void)::sched_yield();
    }
};

static BeforeMain g_before_main;

int main() {
    return 0;
}
