package analysis

import (
	"context"
	"debug/elf"
	"encoding/binary"
	"fmt"
	"sort"
)

type go2seccompAnalyzer struct{}

func NewGo2SeccompAnalyzer() StaticAnalyzer { return &go2seccompAnalyzer{} }

func (*go2seccompAnalyzer) Name() string { return "go2seccomp" }

func (*go2seccompAnalyzer) Analyze(_ context.Context, binaryPath string, _ StaticOptions) ([]uint16, error) {
	f, err := elf.Open(binaryPath)
	if err != nil {
		return nil, fmt.Errorf("open elf: %w", err)
	}
	defer f.Close()

	if !goELFBinary(f) {
		return nil, nil
	}
	if f.Machine != elf.EM_X86_64 {
		return nil, nil
	}

	return scanGoX8664Syscalls(f)
}

func IsGoBinary(binaryPath string) bool {
	f, err := elf.Open(binaryPath)
	if err != nil {
		return false
	}
	defer f.Close()
	return goELFBinary(f)
}

func goELFBinary(f *elf.File) bool {
	return f.Section(".gosymtab") != nil || f.Section(".note.go.buildid") != nil
}

//

//

func scanGoX8664Syscalls(f *elf.File) ([]uint16, error) {
	textSec := f.Section(".text")
	if textSec == nil {
		return nil, fmt.Errorf("no .text section in ELF")
	}
	data, err := textSec.Data()
	if err != nil {
		return nil, fmt.Errorf("read .text: %w", err)
	}

	found := make(map[uint16]struct{})

	callTargets := findSyscallFuncAddrs(f)
	if len(callTargets) > 0 {

		textStart := textSec.Addr
		for i := 0; i+4 < len(data); i++ {
			if data[i] != 0xE8 {
				continue
			}
			rel := int32(binary.LittleEndian.Uint32(data[i+1:]))
			target := textStart + uint64(i) + 5 + uint64(int64(rel))
			if !callTargets[target] {
				continue
			}
			start := i - 128
			if start < 0 {
				start = 0
			}
			if n, ok := extractSyscallNum(data, start, i); ok {
				found[n] = struct{}{}
			}
		}
	} else {

		for i := 1; i < len(data); i++ {
			if data[i-1] != 0x0F || data[i] != 0x05 {
				continue
			}
			start := i - 1 - 128
			if start < 0 {
				start = 0
			}
			if n, ok := extractSyscallNum(data, start, i-1); ok {
				found[n] = struct{}{}
			}
		}
	}

	result := make([]uint16, 0, len(found))
	for n := range found {
		result = append(result, n)
	}
	sort.Slice(result, func(i, j int) bool { return result[i] < result[j] })
	return result, nil
}

func findSyscallFuncAddrs(f *elf.File) map[uint64]bool {
	targets := map[string]bool{
		"syscall.Syscall":         true,
		"syscall.Syscall6":        true,
		"syscall.RawSyscall":      true,
		"syscall.RawSyscall6":     true,
		"syscall.rawVforkSyscall": true,
		"syscall.Syscall9":        true,
	}
	addrs := make(map[uint64]bool)
	syms, err := f.Symbols()
	if err != nil {
		return addrs
	}
	for _, s := range syms {
		if targets[s.Name] {
			addrs[s.Value] = true
		}
	}
	return addrs
}

func extractSyscallNum(data []byte, start, end int) (uint16, bool) {
	for j := end - 1; j >= start; j-- {

		if data[j] == 0xB8 && j+4 < len(data) {
			n := binary.LittleEndian.Uint32(data[j+1:])
			if n <= 512 {
				return uint16(n), true
			}
		}

		if j+6 < len(data) && data[j] == 0x48 && data[j+1] == 0xC7 && data[j+2] == 0xC0 {
			n := binary.LittleEndian.Uint32(data[j+3:])
			if n <= 512 {
				return uint16(n), true
			}
		}

		if j+7 < len(data) && data[j] == 0x48 && data[j+1] == 0xC7 &&
			data[j+2] == 0x04 && data[j+3] == 0x24 {
			n := binary.LittleEndian.Uint32(data[j+4:])
			if n <= 512 {
				return uint16(n), true
			}
		}

		if j+1 < len(data) && (data[j] == 0x31 || data[j] == 0x33) && data[j+1] == 0xC0 {
			return 0, true
		}

		if j+2 < len(data) && data[j] == 0x48 &&
			(data[j+1] == 0x31 || data[j+1] == 0x33) && data[j+2] == 0xC0 {
			return 0, true
		}
	}
	return 0, false
}
