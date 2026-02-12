use std::ffi::{c_char, c_int, c_long};
use std::mem::zeroed;

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
    fn getppid() -> c_int;
    fn getpid() -> c_int;
    fn syscall(num: c_long, ...) -> c_long;
    fn uname(buf: *mut UtsName) -> c_int;
    fn sched_yield() -> c_int;
}

const SYS_GETTID: c_long = 186;

extern "C" fn init_fn() {
    unsafe {
        let _ = getppid();
        let _ = getpid();
        let _ = syscall(SYS_GETTID);

        let mut u: UtsName = zeroed();
        let _ = uname(&mut u as *mut UtsName);
        let _ = sched_yield();
    }
}

#[used]
#[cfg_attr(target_os = "linux", link_section = ".init_array")]
static INIT: extern "C" fn() = init_fn;

fn main() {}