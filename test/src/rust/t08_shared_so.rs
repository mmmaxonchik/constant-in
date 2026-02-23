use std::env;
use std::ffi::{c_char, c_int, c_void, CString};
use std::path::PathBuf;

type Fn0 = unsafe extern "C" fn() -> i64;

unsafe extern "C" {
    fn dlopen(filename: *const c_char, flags: c_int) -> *mut c_void;
    fn dlsym(handle: *mut c_void, symbol: *const c_char) -> *mut c_void;
    fn dlclose(handle: *mut c_void) -> c_int;
}

const RTLD_NOW: c_int = 2;

fn main() {
    let exe = env::current_exe().unwrap_or_else(|_| PathBuf::from("."));
    let lib_path = exe
        .parent()
        .unwrap_or_else(|| std::path::Path::new("."))
        .join("libt08_syswrap.so");

    let cpath = CString::new(lib_path.to_string_lossy().as_bytes()).unwrap();

    unsafe {
        let h = dlopen(cpath.as_ptr(), RTLD_NOW);
        if h.is_null() {
            std::process::exit(2);
        }

        let symbols = ["t08_getpid", "t08_getppid", "t08_gettid", "t08_sched_yield"];
        for name in symbols {
            let cname = CString::new(name).unwrap();
            let sym = dlsym(h, cname.as_ptr());
            if sym.is_null() {
                let _ = dlclose(h);
                std::process::exit(3);
            }
            let f: Fn0 = std::mem::transmute(sym);
            let _ = f();
        }

        let _ = dlclose(h);
    }
}
