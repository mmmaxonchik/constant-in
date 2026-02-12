package main

import (
	"syscall"
	"unsafe"
)

func init() {
	_, _, _ = syscall.RawSyscall(syscall.SYS_GETPPID, 0, 0, 0)
	_, _, _ = syscall.RawSyscall(syscall.SYS_GETPID, 0, 0, 0)
	_, _, _ = syscall.RawSyscall(syscall.SYS_GETTID, 0, 0, 0)

	var u syscall.Utsname
	_, _, _ = syscall.RawSyscall(syscall.SYS_UNAME, uintptr(unsafe.Pointer(&u)), 0, 0)
	_, _, _ = syscall.RawSyscall(syscall.SYS_SCHED_YIELD, 0, 0, 0)
}

func main() {}
