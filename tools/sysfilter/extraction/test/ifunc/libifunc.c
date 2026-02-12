#include <stddef.h>
#include <syscall.h>
#include <unistd.h>

#ifdef DEBUG
#include <stdio.h>
#include <stdlib.h>
#endif

#include "libifunc.h"

#ifdef DEBUG
#define do_sys(x) printf("%d\n", x)
#else
#define do_sys(x) syscall(x)
#endif

void copy_default(char *buf, size_t len) {
    do_sys(600);
}

void copy_sse42(char *buf, size_t len) {
    do_sys(601);
}


void copy_avx2(char *buf, size_t len) {
    do_sys(602);
}


void print_default(char *buf, size_t len) {
    do_sys(700);
}

void print_sse42(char *buf, size_t len) {
    do_sys(701);
}


void print_avx2(char *buf, size_t len) {
    do_sys(702);
}

extern void copy(char *buf, size_t len);

void copy(char *buf, size_t len) __attribute((ifunc ("resolve_copy")));


static void *resolve_copy(void) {
    if (__builtin_cpu_supports("sse4.2")) {
	return copy_sse42;
    } else if (__builtin_cpu_supports("avx2")) {
	return copy_avx2;
    } else {
	return copy_default;
    }
}

extern void print(char *buf, size_t len);

void print(char *buf, size_t len) __attribute((ifunc ("resolve_print")));


static void *resolve_print(void) {
    if (__builtin_cpu_supports("sse4.2")) {
	return print_sse42;
    } else if (__builtin_cpu_supports("avx2")) {
	return print_avx2;
    } else {
	return print_default;
    }
}

void foo(void)
{
    syscall(800);
}
