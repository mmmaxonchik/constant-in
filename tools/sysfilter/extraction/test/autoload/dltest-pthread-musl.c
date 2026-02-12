// dltest-pthread.c
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <unistd.h>

#include <pthread.h>

typedef int (*ptc_func) (pthread_t *thread, const pthread_attr_t *attr,
			 void *(*start_routine)(void *), void *arg);

void *thread_main(void *x)
{
    printf("Hello from thread!\n");

    pthread_exit(0);
}

int main(int argc, char **argv)
{
    pthread_t thread;

    dlerror();
    void* hdl = dlopen("/usr/lib/x86_64-linux-musl/libc.so", RTLD_NOW);
    char *msg = dlerror();

    if (msg != NULL) {
	printf("dlopen encountered error:  %s\n", msg);
	exit(1);
    }

    dlerror();
    ptc_func ptc = dlsym(hdl, "pthread_create");
    msg = dlerror();

    if (msg != NULL) {
	printf("dlsym encountered error:  %s\n", msg);
	exit(1);
    }

    printf("Hello from main!\n");
    ptc(&thread, NULL, &thread_main, 0);

    void *ret;
    pthread_join(thread, &ret);
}
