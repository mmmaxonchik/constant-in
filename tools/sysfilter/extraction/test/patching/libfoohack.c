#define _GNU_SOURCE
#include <dlfcn.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>

#include "sys.h"

#define SYMBOL_OVERRIDE	"func1"
void (*og_func1)(void);

static
__attribute__((constructor))
void init(void)
{
	if ((og_func1 = dlsym(RTLD_NEXT, SYMBOL_OVERRIDE)) == NULL) {
		fprintf(stderr,
				"[ERR] Failed to find %s in (g)libc.\n",
				SYMBOL_OVERRIDE);
		exit(EXIT_FAILURE);
	}
}

void
func0(void)
{
    do_sys(-4);
}

void
func1(void)
{
	do_sys(-5);
	og_func1();
}

void
func3(void)
{
       do_sys(-6);
}
