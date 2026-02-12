package analysis

import "context"

type StaticOptions struct {
	LdPaths           []string
	Sysroot           string
	ConfineFinGrain   bool
	SyspartIcAnalysis bool
}

type DynamicOptions struct {
	ContainerID string
}

type StaticAnalyzer interface {
	Name() string
	Analyze(ctx context.Context, binaryPath string, opts StaticOptions) ([]uint16, error)
}

type DynamicAnalyzer interface {
	Name() string
	Analyze(ctx context.Context, opts DynamicOptions) (*DynamicToolResult, error)
}

type BinaryResult struct {
	Path     string              `json:"path"`
	Combined []uint16            `json:"combined"`
	PerTool  map[string][]uint16 `json:"per_tool"`
}

type DynamicToolResult struct {
	PerExec map[string][]uint16 `json:"per_exec"`

	Executables []string `json:"executables"`

	Syscalls []uint16 `json:"syscalls"`
}

type DynamicResult struct {
	PerTool     map[string]*DynamicToolResult `json:"per_tool,omitempty"`
	Executables []string                      `json:"executables"`
	Syscalls    []uint16                      `json:"syscalls"`
}
