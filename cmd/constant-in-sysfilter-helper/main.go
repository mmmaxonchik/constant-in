package main

/*
#cgo CFLAGS: -I/opt/headers
#cgo LDFLAGS: -L/opt/sysfilter/lib -lsysfilter_analysis

#include "sysfilter_lib.h"
#include <stdlib.h>
*/
import "C"

import (
	"flag"
	"fmt"
	"os"
	"unsafe"
)

func main() {
	binaryPath := flag.String("binary", "", "ELF binary to analyze")
	ldPaths := flag.String("ld-paths", "", "colon-separated library search paths")
	sysroot := flag.String("sysroot", "", "filesystem root for library resolution")
	flag.Parse()

	if *binaryPath == "" {
		fmt.Fprintln(os.Stderr, "--binary is required")
		os.Exit(2)
	}

	syscalls, err := analyze(*binaryPath, *ldPaths, *sysroot)
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
	for _, sc := range syscalls {
		fmt.Println(sc)
	}
}

func analyze(binaryPath, ldPaths, sysroot string) ([]uint16, error) {
	cBin := C.CString(binaryPath)
	defer C.free(unsafe.Pointer(cBin))

	cLD := optionalCString(ldPaths)
	if cLD != nil {
		defer C.free(unsafe.Pointer(cLD))
	}
	cSysroot := optionalCString(sysroot)
	if cSysroot != nil {
		defer C.free(unsafe.Pointer(cSysroot))
	}

	var count C.int
	var cErr *C.char
	ptr := C.sysfilter_analyze(cBin, cLD, cSysroot, &count, &cErr)
	if cErr != nil {
		msg := C.GoString(cErr)
		C.free(unsafe.Pointer(cErr))
		return nil, fmt.Errorf("%s", msg)
	}
	if ptr == nil {
		return nil, nil
	}
	defer C.sysfilter_free_result(ptr)

	raw := unsafe.Slice((*C.uint16_t)(ptr), int(count))
	out := make([]uint16, len(raw))
	for i, v := range raw {
		out[i] = uint16(v)
	}
	return out, nil
}

func optionalCString(s string) *C.char {
	if s == "" {
		return nil
	}
	return C.CString(s)
}
