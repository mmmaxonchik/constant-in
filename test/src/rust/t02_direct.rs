#![allow(non_camel_case_types)]

use std::arch::asm;
use std::ffi::c_char;

type c_long = i64;

#[repr(C)]
struct UtsName {
    sysname: [c_char; 65],
    nodename: [c_char; 65],
    release: [c_char; 65],
    version: [c_char; 65],
    machine: [c_char; 65],
    domainname: [c_char; 65],
}

#[inline(always)]
unsafe fn sc0(n: c_long) -> c_long {
    let ret: c_long;
    unsafe {
        asm!(
            "syscall",
            in("rax") n,
            lateout("rax") ret,
            lateout("rcx") _,
            lateout("r11") _,
            options(nostack)
        );
    }
    ret
}

#[inline(always)]
unsafe fn sc1(n: c_long, a1: c_long) -> c_long {
    let ret: c_long;
    unsafe {
        asm!(
            "syscall",
            in("rax") n,
            in("rdi") a1,
            lateout("rax") ret,
            lateout("rcx") _,
            lateout("r11") _,
            options(nostack)
        );
    }
    ret
}

const SYS_GETPID: c_long = 39;
const SYS_GETPPID: c_long = 110;
const SYS_GETTID: c_long = 186;
const SYS_UNAME: c_long = 63;
const SYS_SCHED_YIELD: c_long = 24;

fn main() {
    unsafe {
        let _ = sc0(SYS_GETPID);
        let _ = sc0(SYS_GETPPID);
        let _ = sc0(SYS_GETTID);

        let mut u = UtsName {
            sysname: [0; 65],
            nodename: [0; 65],
            release: [0; 65],
            version: [0; 65],
            machine: [0; 65],
            domainname: [0; 65],
        };
        let _ = sc1(SYS_UNAME, &mut u as *mut _ as c_long);

        let _ = sc0(SYS_SCHED_YIELD);
    }
}
