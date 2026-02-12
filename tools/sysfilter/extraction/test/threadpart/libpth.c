// libpth.c
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>

void *libpth_do_ext(void *x)
{
    syscall(601);
    return NULL;
}


static void *libpth_do_thing(void *x)
{
    syscall(602);
    return NULL;
}


void libpth_do_thread(void)
{
    pthread_t thread;

    pthread_create(&thread, NULL, libpth_do_thing, (void *)0);

}
