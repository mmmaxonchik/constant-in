unsafe extern "C" {
    fn t10_getpid() -> i64;
    fn t10_getppid() -> i64;
    fn t10_gettid() -> i64;
}

fn main() {
    unsafe {
        let _ = t10_getpid();
        let _ = t10_getppid();
        let _ = t10_gettid();
    }
}
