package main

/*
long t09_getpid(void);
long t09_getppid(void);
long t09_gettid(void);
long t09_sched_yield(void);
*/
import "C"

func main() {
	_ = C.t09_getpid()
	_ = C.t09_getppid()
	_ = C.t09_gettid()
	_ = C.t09_sched_yield()
}
