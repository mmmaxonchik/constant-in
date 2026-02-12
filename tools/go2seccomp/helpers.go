package main

import (
	"debug/elf"
	"encoding/json"
	"fmt"
	"log"
	"os"
	"os/exec"
	"strings"

	"github.com/opencontainers/runtime-spec/specs-go"
)

func openElf(filename string) *elf.File {
	bin, err := os.OpenFile(filename, os.O_RDONLY, 0)
	if err != nil {
		log.Fatalln("can't open file", err)
	}

	f, err := elf.NewFile(bin)
	if err != nil {
		log.Fatalln("elf read error", err)
	}

	return f
}

func isGoBinary(file *elf.File) bool {

	if sect := file.Section(".gosymtab"); sect != nil {
		return true
	}

	if sect := file.Section(".note.go.buildid"); sect != nil {
		return true
	}
	return false
}

func getArch(file *elf.File) specs.Arch {
	var arch specs.Arch

	switch file.Machine.String() {
	case "EM_X86_64":
		arch = specs.ArchX86_64
	case "EM_386":
		arch = specs.ArchX86
	case "EM_ARM":
		arch = specs.ArchARM
	default:
		log.Fatalf("Unsuported arch : %v\n", file.Machine.String())
	}

	fmt.Println("Arch : ", arch)
	return arch
}

func writeProfile(syscallsList []string, arch specs.Arch, profilePath string) {

	profile := specs.LinuxSeccomp{
		DefaultAction: specs.ActErrno,
		Architectures: []specs.Arch{arch},
		Syscalls: []specs.LinuxSyscall{
			specs.LinuxSyscall{
				Names:  syscallsList,
				Action: specs.ActAllow,
			},
		},
	}

	profileFile, err := os.Create(profilePath)
	if err != nil {
		log.Fatalf("Failed to create seccomp profile: %v", err)
	}
	defer profileFile.Close()

	enc := json.NewEncoder(profileFile)
	enc.SetIndent("", "    ")
	enc.Encode(profile)
	fmt.Printf("Saved seccomp profile at %v\n", profilePath)
}

func disassamble(binaryPath string) *os.File {
	disassambled, err := os.Create("disassembled.asm")

	if err != nil {
		log.Fatalf("Failed to disassembling output file, reason: %v", err)
	}

	fmt.Printf("Using go tool objdump to disassemble %v\n", binaryPath)
	cmd := exec.Command("go", "tool", "objdump", binaryPath)
	cmd.Stdout = disassambled
	err = cmd.Run()

	if err != nil {
		log.Fatalf("Couldn't run go tool objdump: %v\n", err)
	}

	disassambled.Seek(0, 0)
	return disassambled
}

func getCallOpByArch(arch specs.Arch) string {
	var j string

	switch arch {
	case specs.ArchX86_64, specs.ArchX86:
		j = "CALL "
	case specs.ArchARM:
		j = "BL "
	default:
		log.Fatalln("Arch not suppported")
	}

	return j
}

func parseFunctionName(instruction string) string {
	texts := strings.Split(instruction, " ")
	currentFunction := texts[1]
	if verbose {
		fmt.Printf("Entering function %v\n", currentFunction)
	}
	return currentFunction
}

func isSyscallPkgCall(arch specs.Arch, instruction string) bool {
	j := getCallOpByArch(arch)
	return strings.Contains(instruction, j+"syscall.Syscall(SB)") || strings.Contains(instruction, j+"syscall.Syscall6(SB)") ||
		strings.Contains(instruction, j+"syscall.RawSyscall(SB)") || strings.Contains(instruction, j+"syscall.RawSyscall6(SB)") ||
		strings.Contains(instruction, j+"syscall.rawVforkSyscall(SB)")
}

func isRuntimeSyscall(arch specs.Arch, instruction, currentFunction string) bool {

	var isRuntimeSC bool
	switch arch {
	case specs.ArchX86:
		isRuntimeSC = (strings.Contains(instruction, "INT $0x80") || strings.Contains(instruction, "SYSENTER"))
	case specs.ArchX86_64:

		isRuntimeSC = strings.Contains(instruction, "SYSCALL") &&
			!strings.Contains(currentFunction, "syscall.Syscall") &&
			!strings.Contains(currentFunction, "syscall.RawSyscall") &&
			!strings.Contains(currentFunction, "syscall.rawVforkSyscall")
	case specs.ArchARM:
		isRuntimeSC = strings.Contains(instruction, "SVC $0") || strings.Contains(instruction, "SWI $0")
	}
	return isRuntimeSC
}

func getDefaultSyscalls(arch specs.Arch) map[int64]bool {
	syscalls := make(map[int64]bool)
	switch arch {

	case specs.ArchX86_64:
		syscalls[202] = true
		syscalls[4] = true
		syscalls[59] = true
	case specs.ArchX86:
		syscalls[240] = true
		syscalls[106] = true
		syscalls[11] = true
	case specs.ArchARM:
		syscalls[240] = true
		syscalls[106] = true
		syscalls[11] = true
	default:
		log.Fatalln(arch, "not supported")
	}

	return syscalls
}
