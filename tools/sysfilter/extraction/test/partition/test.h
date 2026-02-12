#ifndef __TEST_H__
#define __TEST_H__

//#define __USE_GLIBC__

#ifndef __USE_GLIBC__

#define syscall(nr) {	     \
    long __res; \
    asm("syscall;" \
	:"=a"(__res)					\
	:"0"(nr)					\
	:);						\
}

#endif

#endif
