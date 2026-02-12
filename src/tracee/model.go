package tracee

import "encoding/json"

type ContainerInfo struct {
	ID          string `json:"id"`
	Name        string `json:"name"`
	Image       string `json:"image"`
	ImageDigest string `json:"imageDigest"`
}

type ExecutableInfo struct {
	Path string `json:"path"`
}

type TraceeArg struct {
	Name  string          `json:"name"`
	Type  string          `json:"type"`
	Value json.RawMessage `json:"value"`
}

type TraceeEvent struct {
	Timestamp       uint64         `json:"timestamp"`
	ProcessID       int            `json:"processId"`
	ThreadID        int            `json:"threadId"`
	ParentProcessID int            `json:"parentProcessId"`
	HostProcessID   int            `json:"hostProcessId"`
	HostThreadID    int            `json:"hostThreadId"`
	ProcessName     string         `json:"processName"`
	EventName       string         `json:"eventName"`
	Syscall         string         `json:"syscall"`
	ContainerID     string         `json:"containerId"`
	Container       ContainerInfo  `json:"container"`
	Executable      ExecutableInfo `json:"executable"`
	Args            []TraceeArg    `json:"args"`
}

type RuntimeResult struct {
	ContainerExecs map[string]struct{}

	ContainerSyscalls map[string]struct{}

	PerExecSyscalls map[string]map[string]struct{}
}
