use std::ffi::{c_char, c_int, c_long};
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
    fn syscall(num: c_long, ...) -> c_long;
}

const SYS_GETTID: c_long = 186;
const SYS_GETUID: c_long = 102;
const SYS_GETGID: c_long = 104;
const SYS_UMASK: c_long = 95;
const SYS_SCHED_YIELD: c_long = 24;

fn branch_target() {
    unsafe {
        let _ = syscall(SYS_GETTID);
        let _ = syscall(SYS_GETUID);
        let _ = syscall(SYS_GETGID);
        let _ = syscall(SYS_UMASK, 0o22);
        let _ = syscall(SYS_SCHED_YIELD);
    }
}

#[used]
static ANCHOR: [fn(); 1] = [branch_target];

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

    let never = false;
    if never {
        (ANCHOR[0])();
    }
}