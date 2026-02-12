package pipeline

import (
	"encoding/json"
	"fmt"
	"sort"

	"github.com/mmmaxonchik/constant-in/src/syscalls"
)

type SeccompProfile struct {
	DefaultAction string           `json:"defaultAction"`
	Architectures []string         `json:"architectures"`
	Syscalls      []SeccompSyscall `json:"syscalls"`
}

type SeccompSyscall struct {
	Names  []string `json:"names"`
	Action string   `json:"action"`
	Args   []any    `json:"args"`
}

var DefaultDockerSyscalls = []string{
	"openat", "newfstatat", "fstat", "statx", "getcwd", "chdir", "fstatfs",
	"access", "close", "fcntl", "read", "write", "capget", "capset",
	"futex", "getdents64", "getppid", "prctl", "setgid", "setgroups",
	"setuid", "readlinkat",
}

var syscallUpgrades = map[string][]string{
	"epoll_wait": {"epoll_pwait", "epoll_pwait2"},
	"poll":       {"ppoll"},
	"select":     {"pselect"},
}

func buildProfile(nums map[uint16]struct{}, defaultSyscalls []string) *SeccompProfile {
	nameSet := make(map[string]struct{}, len(nums)+len(defaultSyscalls))
	for num := range nums {
		if name, ok := syscalls.Name(num); ok {
			nameSet[name] = struct{}{}
		}
	}
	for _, name := range defaultSyscalls {
		if name != "" {
			nameSet[name] = struct{}{}
		}
	}

	for legacy, upgrades := range syscallUpgrades {
		if _, ok := nameSet[legacy]; ok {
			for _, u := range upgrades {
				nameSet[u] = struct{}{}
			}
		}
	}

	names := make([]string, 0, len(nameSet))
	for name := range nameSet {
		names = append(names, name)
	}
	sort.Strings(names)

	return &SeccompProfile{
		DefaultAction: "SCMP_ACT_ERRNO",
		Architectures: []string{
			"SCMP_ARCH_X86_64",
			"SCMP_ARCH_X86",
			"SCMP_ARCH_X32",
		},
		Syscalls: []SeccompSyscall{
			{
				Names:  names,
				Action: "SCMP_ACT_ALLOW",
				Args:   []any{},
			},
		},
	}
}

func BuildSeccompProfile(r *Result, defaultSyscalls []string) (*SeccompProfile, error) {
	if r == nil {
		return nil, fmt.Errorf("nil result")
	}
	union := make(map[uint16]struct{})
	for _, ce := range r.Combined {
		for _, n := range ce.Syscalls {
			union[n] = struct{}{}
		}
	}
	return buildProfile(union, defaultSyscalls), nil
}

func BuildSeccompProfileForTool(r *Result, tool string, defaultSyscalls []string) (*SeccompProfile, error) {
	if r == nil {
		return nil, fmt.Errorf("nil result")
	}
	union := make(map[uint16]struct{})
	for _, n := range r.PerTool[tool] {
		union[n] = struct{}{}
	}
	return buildProfile(union, defaultSyscalls), nil
}

func AllToolNames(r *Result) []string {
	names := make([]string, 0, len(r.PerTool))
	for name := range r.PerTool {
		names = append(names, name)
	}
	sort.Strings(names)
	return names
}

func MarshalSeccompProfile(p *SeccompProfile) ([]byte, error) {
	return json.MarshalIndent(p, "", "  ")
}
