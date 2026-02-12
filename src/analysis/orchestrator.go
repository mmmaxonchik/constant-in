package analysis

import (
	"context"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"sort"

	"github.com/mmmaxonchik/constant-in/src/logger"
)

func isELF(path string) bool {
	f, err := os.Open(path)
	if err != nil {
		return false
	}
	defer f.Close()
	var magic [4]byte
	_, err = io.ReadFull(f, magic[:])
	return err == nil && magic[0] == 0x7f && magic[1] == 'E' && magic[2] == 'L' && magic[3] == 'F'
}

func RunStatic(
	ctx context.Context,
	binaryPath string,
	analyzers []StaticAnalyzer,
	opts StaticOptions,
	log logger.Logger,
) (*BinaryResult, error) {
	if !isELF(binaryPath) {
		return nil, fmt.Errorf("not an ELF binary: %s", binaryPath)
	}

	type staticToolResult struct {
		name     string
		syscalls []uint16
		err      error
	}

	base := filepath.Base(binaryPath)
	ch := make(chan staticToolResult, len(analyzers))

	for _, a := range analyzers {
		a := a
		go func() {
			sc, err := a.Analyze(ctx, binaryPath, opts)
			ch <- staticToolResult{name: a.Name(), syscalls: sc, err: err}
		}()
	}

	results := make([]staticToolResult, 0, len(analyzers))
	for range analyzers {
		results = append(results, <-ch)
	}

	perTool := make(map[string][]uint16, len(analyzers))
	set := make(map[uint16]struct{})

	for _, r := range results {
		if r.err != nil {
			log.Warnf("%s: failed for %s: %v", r.name, base, r.err)
			continue
		}
		log.Infof("%s: %d syscalls for %s", r.name, len(r.syscalls), base)
		perTool[r.name] = r.syscalls
		for _, v := range r.syscalls {
			set[v] = struct{}{}
		}
	}

	combined := make([]uint16, 0, len(set))
	for k := range set {
		combined = append(combined, k)
	}
	sort.Slice(combined, func(i, j int) bool { return combined[i] < combined[j] })

	return &BinaryResult{
		Path:     binaryPath,
		Combined: combined,
		PerTool:  perTool,
	}, nil
}

func RunDynamic(
	ctx context.Context,
	analyzers []DynamicAnalyzer,
	opts DynamicOptions,
	log logger.Logger,
) (*DynamicResult, error) {
	type dynToolResult struct {
		name string
		res  *DynamicToolResult
		err  error
	}

	ch := make(chan dynToolResult, len(analyzers))
	for _, a := range analyzers {
		a := a
		go func() {
			res, err := a.Analyze(ctx, opts)
			ch <- dynToolResult{name: a.Name(), res: res, err: err}
		}()
	}

	perTool := make(map[string]*DynamicToolResult, len(analyzers))
	execSet := make(map[string]struct{})
	syscallSet := make(map[uint16]struct{})
	var firstErr error

	for range analyzers {
		r := <-ch
		if r.err != nil {
			log.Warnf("%s: dynamic analysis failed: %v", r.name, r.err)
			if firstErr == nil {
				firstErr = r.err
			}
			continue
		}
		log.Infof("%s: %d executables, %d syscalls for container %s",
			r.name, len(r.res.Executables), len(r.res.Syscalls), opts.ContainerID)
		perTool[r.name] = r.res
		for _, e := range r.res.Executables {
			execSet[e] = struct{}{}
		}
		for _, sc := range r.res.Syscalls {
			syscallSet[sc] = struct{}{}
		}
	}

	if len(perTool) == 0 {
		return nil, fmt.Errorf("all dynamic analyzers failed: %w", firstErr)
	}

	execs := make([]string, 0, len(execSet))
	for k := range execSet {
		execs = append(execs, k)
	}
	sort.Strings(execs)

	syscalls := make([]uint16, 0, len(syscallSet))
	for k := range syscallSet {
		syscalls = append(syscalls, k)
	}
	sort.Slice(syscalls, func(i, j int) bool { return syscalls[i] < syscalls[j] })

	return &DynamicResult{
		PerTool:     perTool,
		Executables: execs,
		Syscalls:    syscalls,
	}, nil
}
