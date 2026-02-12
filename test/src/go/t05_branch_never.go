package main

import (
	"os"
	"runtime"
	"syscall"
	"time"
	"unsafe"
)

func branchTarget() {
	_, _, _ = syscall.RawSyscall(syscall.SYS_GETTID, 0, 0, 0)
	_, _, _ = syscall.RawSyscall(syscall.SYS_GETUID, 0, 0, 0)
	_, _, _ = syscall.RawSyscall(syscall.SYS_GETGID, 0, 0, 0)
	_, _, _ = syscall.RawSyscall(syscall.SYS_UMASK, 022, 0, 0)
	_, _, _ = syscall.RawSyscall(syscall.SYS_SCHED_YIELD, 0, 0, 0)
}

func main() {
	_ = os.Getpid()
	_ = os.Getppid()

	var u syscall.Utsname
	_ = syscall.Uname(&u)

	time.Sleep(13 * time.Millisecond)
	runtime.Gosched()

	never := false
	if never {
		branchTarget()
	}

	_ = unsafe.Pointer(nil)
}
