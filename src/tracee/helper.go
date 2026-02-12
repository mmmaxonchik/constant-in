package tracee

import (
	"bufio"
	"encoding/json"
	"fmt"
	"io"
)

func ReadTraceeNDJSON(r io.Reader, containerId string) (*RuntimeResult, error) {
	scanner := bufio.NewScanner(r)
	buf := make([]byte, 0, 1024*1024)
	scanner.Buffer(buf, 16*1024*1024)

	result := RuntimeResult{
		ContainerExecs:    make(map[string]struct{}),
		ContainerSyscalls: make(map[string]struct{}),
		PerExecSyscalls:   make(map[string]map[string]struct{}),
	}

	pidToExec := make(map[int]string)

	for scanner.Scan() {
		line := scanner.Bytes()
		if len(line) == 0 {
			continue
		}

		var ev TraceeEvent
		if err := json.Unmarshal(line, &ev); err != nil {
			return nil, fmt.Errorf("unmarshal tracee line: %w", err)
		}
		if ev.ContainerID != containerId {
			continue
		}

		switch ev.EventName {
		case "sched_process_exec":
			for _, a := range ev.Args {
				if a.Name == "pathname" {
					var path string
					if err := json.Unmarshal(a.Value, &path); err != nil {

						path = string(a.Value)
						if len(path) >= 2 && path[0] == '"' && path[len(path)-1] == '"' {
							path = path[1 : len(path)-1]
						}
					}
					if path == "" {
						continue
					}
					pidToExec[ev.HostProcessID] = path
					result.ContainerExecs[path] = struct{}{}
					if _, ok := result.PerExecSyscalls[path]; !ok {
						result.PerExecSyscalls[path] = make(map[string]struct{})
					}
				}
			}

		case "sys_enter":
			if ev.Syscall == "" {
				continue
			}
			result.ContainerSyscalls[ev.Syscall] = struct{}{}
			exec := pidToExec[ev.HostProcessID]
			if exec == "" {
				exec = ev.Executable.Path
			}
			if exec == "" {
				continue
			}
			if _, ok := result.PerExecSyscalls[exec]; !ok {
				result.PerExecSyscalls[exec] = make(map[string]struct{})
			}
			result.PerExecSyscalls[exec][ev.Syscall] = struct{}{}
		}
	}

	if err := scanner.Err(); err != nil {
		return nil, fmt.Errorf("scan tracee stream: %w", err)
	}

	return &result, nil
}
