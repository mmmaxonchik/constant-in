package analysis

import (
	"context"
	"fmt"
	"os"
	"sort"

	"github.com/mmmaxonchik/constant-in/src/syscalls"
	"github.com/mmmaxonchik/constant-in/src/tracee"
)

type traceeAnalyzer struct {
	logPath string
}

func NewTraceeAnalyzer(logPath string) DynamicAnalyzer {
	return &traceeAnalyzer{logPath: logPath}
}

func (*traceeAnalyzer) Name() string { return "tracee" }

func (t *traceeAnalyzer) Analyze(_ context.Context, opts DynamicOptions) (*DynamicToolResult, error) {
	f, err := os.Open(t.logPath)
	if err != nil {
		return nil, fmt.Errorf("open log %q: %w", t.logPath, err)
	}
	defer f.Close()

	result, err := tracee.ReadTraceeNDJSON(f, opts.ContainerID)
	if err != nil {
		return nil, err
	}

	perExec := make(map[string][]uint16, len(result.PerExecSyscalls))
	allSyscallSet := make(map[uint16]struct{})

	for execPath, nameSet := range result.PerExecSyscalls {
		nums := make([]uint16, 0, len(nameSet))
		for name := range nameSet {
			if n, ok := syscalls.Number(name); ok {
				nums = append(nums, n)
				allSyscallSet[n] = struct{}{}
			} else {
				return nil, fmt.Errorf("failed to resolve syscall name in table:", name)
			}
		}
		sort.Slice(nums, func(i, j int) bool { return nums[i] < nums[j] })
		perExec[execPath] = nums
	}

	execs := make([]string, 0, len(result.ContainerExecs))
	for k := range result.ContainerExecs {
		execs = append(execs, k)
	}
	sort.Strings(execs)

	for name := range result.ContainerSyscalls {
		if n, ok := syscalls.Number(name); ok {
			allSyscallSet[n] = struct{}{}
		} else {
			return nil, fmt.Errorf("failed to resolve syscall name in table:", name)
		}
	}

	allSyscalls := make([]uint16, 0, len(allSyscallSet))
	for n := range allSyscallSet {
		allSyscalls = append(allSyscalls, n)
	}
	sort.Slice(allSyscalls, func(i, j int) bool { return allSyscalls[i] < allSyscalls[j] })

	return &DynamicToolResult{
		PerExec:     perExec,
		Executables: execs,
		Syscalls:    allSyscalls,
	}, nil
}
