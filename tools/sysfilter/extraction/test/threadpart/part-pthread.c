// argtrack.c
#include <pthread.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <fcntl.h>

void *libpth_do_ext(void *x);

__attribute__((noinline))
int do_nothing(void)
{
    return 0;
}

void *do_thing(void *x)
{
    syscall(600);
    //pthread_exit(NULL);
    return NULL;
}

// Should be able to resolve a PLT call
__attribute__((noinline))
void do_plt(void)
{
    pthread_t thread;
    pthread_create(&thread, NULL, libpth_do_ext, (void *)0);
}

// Should not be able to resolve a call through an argument
__attribute__((noinline))
void do_thing_wrap(void * (*thread_func) (void *))
{
    pthread_t thread;
    pthread_create(&thread, NULL, thread_func, (void *)0);
}

int main(int argc, char **argv)
{
    pthread_t thread;

    pthread_create(&thread, NULL, do_thing, (void *)0);
    pthread_join(thread, NULL);

    do_plt();

    do_thing_wrap(do_thing);

    return do_nothing();

}
