extern "C" long t10_getpid(void);
extern "C" long t10_getppid(void);
extern "C" long t10_gettid(void);

int main() {
    (void)t10_getpid();
    (void)t10_getppid();
    (void)t10_gettid();
    return 0;
}
