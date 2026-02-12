package main

import (
	"os"
	"runtime"
	"syscall"
	"time"
)

func existsOnly() {
	_ = os.Getuid()
	_ = os.Geteuid()
	_ = os.Getgid()
	_ = os.Getegid()
	_ = syscall.Umask(022)
}

var sink = []func(){existsOnly}

func main() {
	_ = os.Getpid()
	_ = os.Getppid()
	var u syscall.Utsname
	_ = syscall.Uname(&u)
	time.Sleep(13 * time.Millisecond)
	runtime.Gosched()

	_ = sink[0]
}