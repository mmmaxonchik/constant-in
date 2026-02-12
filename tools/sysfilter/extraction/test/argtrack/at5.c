// argtrack.c
#include <pthread.h>

void thr(void)
{
    pthread_exit(NULL);
}

void __attribute__((noinline)) actual(void)
{
    pthread_t thread;
    pthread_attr_t attr;

    pthread_create(&thread, &attr, (void*)thr, 0);

}

void __attribute__((noinline)) fake(void)
{
    pthread_t thread;
    pthread_attr_t attr;

    pthread_create(&thread, &attr, (void*)42, 0);

}

int main(int argc, char **argv)
{
    actual();
    fake();
    return 0;
}
