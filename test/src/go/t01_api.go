package main

import (
	"runtime"
	"syscall"
	"time"
	"os"
)

func main() {
	_ = os.Getpid()
	_ = os.Getppid()

	var u syscall.Utsname
	_ = syscall.Uname(&u)

	time.Sleep(13 * time.Millisecond)
	runtime.Gosched()


}
