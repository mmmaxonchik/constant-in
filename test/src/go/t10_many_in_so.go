package main

/*
long t10_getpid(void);
long t10_getppid(void);
long t10_gettid(void);
*/
import "C"

func main() {
	_ = C.t10_getpid()
	_ = C.t10_getppid()
	_ = C.t10_gettid()
}
