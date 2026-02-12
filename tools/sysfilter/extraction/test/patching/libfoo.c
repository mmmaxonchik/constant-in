#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>

#include "sys.h"

void
func0(void)
{
	do_sys(-1);
}

void
func1(void)
{
	do_sys(-2);
}

void
func2(void)
{
	do_sys(-3);
}

void
func10(void)
{
    do_sys(-10);
}
