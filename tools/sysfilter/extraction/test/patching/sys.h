#ifndef __TEST_SYS_H__
#define __TEST_SYS_H__

#ifdef DEBUG
#define do_sys(x) printf("%s:%d:  %d\n", __FILE__, __LINE__, x)
#else
#define do_sys(x) syscall(x)
#endif



#endif
