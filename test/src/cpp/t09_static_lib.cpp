extern "C" long t09_getpid(void);
extern "C" long t09_getppid(void);
extern "C" long t09_gettid(void);
extern "C" long t09_sched_yield(void);

int main() {
    (void)t09_getpid();
    (void)t09_getppid();
    (void)t09_gettid();
    (void)t09_sched_yield();
    return 0;
}
