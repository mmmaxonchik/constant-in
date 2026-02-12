// at8.c
#define _GNU_SOURCE

#include <sched.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/syscall.h>

void do_child(void)
{
    syscall(601);
}

void do_fork(void)
{
    pid_t pid = fork();

    if (pid) {
	do_child();
    }

    syscall(602);
}


int clone_child(void *arg)
{
    syscall(603);

    return 0;
}

void do_clone(void)
{
    unshare(42);
    clone(clone_child, NULL, 0, NULL);
}

int main(int argc, char **argv)
{
    syscall(600);
    do_fork();
    do_clone();

    return 0;
}
