use std::ffi::{c_char, c_int, c_uint};
use std::mem::zeroed;
use std::thread;
use std::time::Duration;

#[repr(C)]
struct UtsName {
    sysname: [c_char; 65],
    nodename: [c_char; 65],
    release: [c_char; 65],
    version: [c_char; 65],
    machine: [c_char; 65],
    domainname: [c_char; 65],
}

unsafe extern "C" {
    fn getpid() -> c_int;
    fn getppid() -> c_int;
    fn uname(buf: *mut UtsName) -> c_int;
    fn sched_yield() -> c_int;

    fn getuid() -> c_uint;
    fn geteuid() -> c_uint;
    fn getgid() -> c_uint;
    fn getegid() -> c_uint;
    fn umask(mask: c_uint) -> c_uint;
}

fn exists_only() {
    unsafe {
        let _ = getuid();
        let _ = geteuid();
        let _ = getgid();
        let _ = getegid();
        let _ = umask(0o22);
    }
}

#[used]
static SINK: [fn(); 1] = [exists_only];

fn main() {
    unsafe {
        let _ = getpid();
        let _ = getppid();

        let mut u: UtsName = zeroed();
        let _ = uname(&mut u as *mut UtsName);
    }

    thread::sleep(Duration::from_millis(13));

    unsafe {
        let _ = sched_yield();
    }

    let _ = SINK[0] as usize;
}