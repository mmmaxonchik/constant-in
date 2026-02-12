package main

import "syscall"

func main() {
	fds, _ := syscall.Socketpair(syscall.AF_UNIX, syscall.SOCK_STREAM, 0)
	buf := []byte{'A'}

	_ = syscall.Sendto(fds[0], buf, 0, nil)
	_, _, _ = syscall.Recvfrom(fds[1], buf, 0)

	_ = syscall.Shutdown(fds[0], syscall.SHUT_RDWR)
	_ = syscall.Close(fds[0])
	_ = syscall.Close(fds[1])
}