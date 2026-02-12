package main

import (
	"syscall"
	"unsafe"
)

func sc0(n uintptr) uintptr {
	r1, _, _ := syscall.RawSyscall(n, 0, 0, 0)
	return r1
}
func sc1(n, a1 uintptr) uintptr {
	r1, _, _ := syscall.RawSyscall(n, a1, 0, 0)
	return r1
}

func main() {
	_ = sc0(syscall.SYS_GETPID)
	_ = sc0(syscall.SYS_GETPPID)
	_ = sc0(syscall.SYS_GETTID)
	var u syscall.Utsname
	_ = sc1(syscall.SYS_UNAME, uintptr(unsafe.Pointer(&u)))
	_, _, _ = syscall.RawSyscall(syscall.SYS_SCHED_YIELD, 0, 0, 0)
}
