#![allow(non_camel_case_types)]

use std::ffi::{c_int, c_void};

type size_t = usize;
type ssize_t = isize;

unsafe extern "C" {
    fn socketpair(domain: c_int, typ: c_int, protocol: c_int, sv: *mut c_int) -> c_int;
    fn sendto(
        sockfd: c_int,
        buf: *const c_void,
        len: size_t,
        flags: c_int,
        dest_addr: *const c_void,
        addrlen: c_int,
    ) -> ssize_t;
    fn recvfrom(
        sockfd: c_int,
        buf: *mut c_void,
        len: size_t,
        flags: c_int,
        src_addr: *mut c_void,
        addrlen: *mut c_int,
    ) -> ssize_t;
    fn shutdown(sockfd: c_int, how: c_int) -> c_int;
    fn close(fd: c_int) -> c_int;
}

const AF_UNIX: c_int = 1;
const SOCK_STREAM: c_int = 1;
const SHUT_RDWR: c_int = 2;

fn main() {
    unsafe {
        let mut fds = [0 as c_int; 2];
        let _ = socketpair(AF_UNIX, SOCK_STREAM, 0, fds.as_mut_ptr());

        let mut buf = [b'A'; 1];
        let _ = sendto(
            fds[0],
            buf.as_ptr() as *const c_void,
            1,
            0,
            std::ptr::null(),
            0,
        );
        let _ = recvfrom(
            fds[1],
            buf.as_mut_ptr() as *mut c_void,
            1,
            0,
            std::ptr::null_mut(),
            std::ptr::null_mut(),
        );

        let _ = shutdown(fds[0], SHUT_RDWR);
        let _ = close(fds[0]);
        let _ = close(fds[1]);
    }
}