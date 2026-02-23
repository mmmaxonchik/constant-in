unsafe extern "C" {
    fn t09_getpid() -> i64;
    fn t09_getppid() -> i64;
    fn t09_gettid() -> i64;
    fn t09_sched_yield() -> i64;
}

fn main() {
    unsafe {
        let _ = t09_getpid();
        let _ = t09_getppid();
        let _ = t09_gettid();
        let _ = t09_sched_yield();
    }
}
