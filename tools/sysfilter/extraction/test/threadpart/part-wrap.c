// part-wrap.c
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

// *********** 1. Thread function calls another function passed as an argument ******
// Expected result: tracking the argument to the thread function
// should work, as it involves the same procedure as looking up the
// function itself
void do_syscall(void)
{
    syscall(603);
}

void *run_as_thread(void *arg)
{
    void (*thread_func)(void) = arg;
    thread_func();

    return NULL;
}

void do_simple(void)
{
    pthread_t thread;
    pthread_create(&thread, NULL, run_as_thread, (void *)do_syscall);
    pthread_join(thread, NULL);
}


// ************* 2. Thread function is an element of a data structure *********
// Expected result:  this should also work:  we can recover a constant address
// and use the DataSymbolList to locate any code pointers that are referenced
// by the DataSymbol thread_data
struct thread_data {
    int x;
    void (*func)(void);
    int y;
};

void do_data_syscall(void)
{
    syscall(604);
}

void *run_as_thread_data(void *arg)
{
    struct thread_data *data = (struct thread_data*)arg;
    data->func();

    return NULL;
}

struct thread_data thread_data = {
    42,
    do_data_syscall,
    0x1004,
};

void do_wrap_data(void)
{
    pthread_t thread;
    pthread_create(&thread, NULL, run_as_thread_data, (void *)&thread_data);
    pthread_join(thread, NULL);
}

// ********** 3. Try to recover argument when just used as an integer *********
// Expected result:  we should be able to recover the integer, but we won't
// be able to resolve it to a symbol (assuming that 0x1070 is not in the memory map)
// this will fail with the status flag RT_ADDR_SYMBOL_RESOLUTION_FAILED
void *bogus_thread(void *arg)
{
    return NULL;
}

void do_bogus(void)
{
    pthread_t thread;
    pthread_create(&thread, NULL, bogus_thread, (void *)0x1070);
    pthread_join(thread, NULL);
}


// ************* 4. Multiple thread functions in one data structure *********
// Expected result: we should recover *two* results for the argument,
// one for each function pointer in the structure
struct thread_multi_data {
    int x;
    void (*func)(void);
    void* (*lib_func)(void*);
    int y;
};

struct thread_multi_data tm_data = {
    42,
    do_data_syscall,
    libpth_do_ext,
    0x1004,
};

void *run_as_thread_multi_data(void *arg)
{
    struct thread_multi_data *data = (struct thread_multi_data*)arg;
    data->func();
    data->lib_func(NULL);

    return NULL;
}


void do_wrap_multi_data(void)
{
    pthread_t thread;
    pthread_create(&thread, NULL, run_as_thread_multi_data, (void *)&tm_data);
    pthread_join(thread, NULL);
}


// ************ 5. Thread function is an element of a non-global data structure *******
// Expected result:  this should fail, since tdata is located on the stack,
// rather than in global data like examples 2 and 4
void do_wrap_data_local(void)
{
    struct thread_data tdata = {
	1,
	do_data_syscall,
	0,
    };

    pthread_t thread;
    pthread_create(&thread, NULL, run_as_thread_data, (void *)&tdata);
    pthread_join(thread, NULL);
}

// *********** 6. Thread function is stored in a local variable ********
void do_local(int x)
{
    void *(*tfunc)(void*) = (x == 0) ? bogus_thread : run_as_thread;
    pthread_t thread;
    pthread_create(&thread, NULL, tfunc, (void *)0);
    pthread_join(thread, NULL);
}


int main(int argc, char **argv)
{
    do_simple();
    do_wrap_data();
    do_bogus();
    do_wrap_multi_data();
    do_wrap_data_local();
    do_local(argc);

    return 0;
}
