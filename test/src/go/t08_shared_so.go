package main

/*
#cgo LDFLAGS: -ldl
#include <dlfcn.h>
#include <stdlib.h>

typedef long (*fn0)(void);

static long call_symbol(void *h, const char *name) {
    fn0 fn = (fn0)dlsym(h, name);
    if (!fn) {
        return -1;
    }
    return fn();
}
*/
import "C"

import (
	"os"
	"path/filepath"
	"unsafe"
)

func main() {
	exe, err := filepath.Abs(os.Args[0])
	if err != nil {
		os.Exit(1)
	}
	libPath := filepath.Join(filepath.Dir(exe), "libt08_syswrap.so")

	cpath := C.CString(libPath)
	defer C.free(unsafe.Pointer(cpath))

	h := C.dlopen(cpath, C.RTLD_NOW)
	if h == nil {
		os.Exit(2)
	}
	defer C.dlclose(h)

	names := []string{"t08_getpid", "t08_getppid", "t08_gettid", "t08_sched_yield"}
	for _, n := range names {
		cname := C.CString(n)
		ret := C.call_symbol(h, cname)
		C.free(unsafe.Pointer(cname))
		if ret == -1 {
			os.Exit(3)
		}
	}
}
