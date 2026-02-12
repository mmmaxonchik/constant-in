long t09_getpid(void);
long t09_getppid(void);
long t09_gettid(void);
long t09_sched_yield(void);

int main(void) {
    (void)t09_getpid();
    (void)t09_getppid();
    (void)t09_gettid();
    (void)t09_sched_yield();
    return 0;
}
