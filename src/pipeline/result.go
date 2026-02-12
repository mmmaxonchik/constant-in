package pipeline

import "encoding/json"

type BinaryEntry struct {
	Path     string
	Syscalls map[string][]uint16
}

func (b BinaryEntry) MarshalJSON() ([]byte, error) {
	m := make(map[string]any, len(b.Syscalls)+1)
	m["path"] = b.Path
	for tool, sc := range b.Syscalls {
		m[tool] = sc
	}
	return json.Marshal(m)
}

type CombinedEntry struct {
	Path     string
	Syscalls []uint16
}

func (c CombinedEntry) MarshalJSON() ([]byte, error) {
	return json.Marshal(map[string][]uint16{c.Path: c.Syscalls})
}

type Result struct {
	Mode  string `json:"mode"`
	Image string `json:"image,omitempty"`

	Static []BinaryEntry `json:"static,omitempty"`

	Dynamic []BinaryEntry `json:"dynamic,omitempty"`

	Combined []CombinedEntry `json:"combined,omitempty"`

	PerTool map[string][]uint16 `json:"per_tool,omitempty"`
}
