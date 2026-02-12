package main

import (
	"syscall"
	"unsafe"
)

const (
	SYS_CLOSE        = 3
	SYS_MMAP         = 9
	SYS_MPROTECT     = 10
	SYS_MUNMAP       = 11
	SYS_FTRUNCATE    = 77
	SYS_MEMFD_CREATE = 319
)

func main() {
	name := []byte("x\x00")
	fd, _, _ := syscall.RawSyscall(SYS_MEMFD_CREATE, uintptr(unsafe.Pointer(&name[0])), 0, 0)
	_, _, _ = syscall.RawSyscall(SYS_FTRUNCATE, fd, 4096, 0)
	p, _, _ := syscall.RawSyscall6(SYS_MMAP, 0, 4096, 3, 2, fd, 0)
	_, _, _ = syscall.RawSyscall(SYS_MPROTECT, p, 4096, 1)
	_, _, _ = syscall.RawSyscall(SYS_MUNMAP, p, 4096, 0)
	_, _, _ = syscall.RawSyscall(SYS_CLOSE, fd, 0, 0)
}
