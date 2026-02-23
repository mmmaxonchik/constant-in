long t10_getpid(void);
long t10_getppid(void);
long t10_gettid(void);

int main(void) {
    (void)t10_getpid();
    (void)t10_getppid();
    (void)t10_gettid();
    return 0;
}
