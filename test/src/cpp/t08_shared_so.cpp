#include <dlfcn.h>
#include <limits.h>
#include <libgen.h>
#include <cstdio>
#include <cstdlib>

typedef long (*fn0)();

static void make_lib_path(const char *argv0, char *out, size_t out_sz) {
    char tmp[PATH_MAX];
    if (!realpath(argv0, tmp)) {
        std::snprintf(tmp, sizeof(tmp), ".");
    }
    char *dir = dirname(tmp);
    std::snprintf(out, out_sz, "%s/libt08_syswrap.so", dir);
}

int main(int argc, char **argv) {
    (void)argc;
    char lib_path[PATH_MAX];
    make_lib_path(argv[0], lib_path, sizeof(lib_path));

    void *h = dlopen(lib_path, RTLD_NOW);
    if (!h) return 1;

    fn0 getpid_fn = reinterpret_cast<fn0>(dlsym(h, "t08_getpid"));
    fn0 getppid_fn = reinterpret_cast<fn0>(dlsym(h, "t08_getppid"));
    fn0 gettid_fn = reinterpret_cast<fn0>(dlsym(h, "t08_gettid"));
    fn0 sched_yield_fn = reinterpret_cast<fn0>(dlsym(h, "t08_sched_yield"));

    if (!getpid_fn || !getppid_fn || !gettid_fn || !sched_yield_fn) {
        (void)dlclose(h);
        return 2;
    }

    (void)getpid_fn();
    (void)getppid_fn();
    (void)gettid_fn();
    (void)sched_yield_fn();

    (void)dlclose(h);
    return 0;
}
