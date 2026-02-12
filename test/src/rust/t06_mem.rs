#![allow(non_camel_case_types)]

type c_long = i64;
type size_t = usize;

extern "C" {
    fn syscall(num: c_long, ...) -> c_long;
}

const SYS_MEMFD_CREATE: c_long = 319;
const SYS_FTRUNCATE: c_long = 77;
const SYS_MMAP: c_long = 9;
const SYS_MPROTECT: c_long = 10;
const SYS_MUNMAP: c_long = 11;
const SYS_CLOSE: c_long = 3;

const PROT_READ: c_long = 1;
const PROT_WRITE: c_long = 2;
const MAP_PRIVATE: c_long = 2;

fn main() {
    unsafe {
        let name = b"x\0";
        let fd = syscall(SYS_MEMFD_CREATE, name.as_ptr(), 0) as c_long;

        let _ = syscall(SYS_FTRUNCATE, fd, 4096);

        let p = syscall(
            SYS_MMAP,
            0 as *const core::ffi::c_void,
            4096 as size_t,
            (PROT_READ | PROT_WRITE) as c_long,
            MAP_PRIVATE as c_long,
            fd as c_long,
            0 as c_long,
        ) as c_long;

        
        let _ = syscall(SYS_MPROTECT, p as *const core::ffi::c_void, 4096 as size_t, PROT_READ as c_long);
        let _ = syscall(SYS_MUNMAP, p as *const core::ffi::c_void, 4096 as size_t);

        let _ = syscall(SYS_CLOSE, fd as c_long);
    }
}
