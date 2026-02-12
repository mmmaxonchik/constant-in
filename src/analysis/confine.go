package analysis

import (
	"bufio"
	"bytes"
	"context"
	"fmt"
	"os/exec"
	"strconv"
	"strings"
)

const (
	confineScript     = "/opt/confine/confine_run.py"
	confineGlibcGraph = "/opt/confine/libc-callgraphs/glibc.callgraph"
	confineMuslGraph  = "/opt/confine/libc-callgraphs/musllibc.callgraph"
)

type confineAnalyzer struct{}

func NewConfineAnalyzer() StaticAnalyzer { return &confineAnalyzer{} }

func (*confineAnalyzer) Name() string { return "confine" }

func (*confineAnalyzer) Analyze(ctx context.Context, binaryPath string, opts StaticOptions) ([]uint16, error) {
	argv := []string{
		"python3",
		confineScript,
		"--callgraph", confineGlibcGraph,
		"--musl-callgraph", confineMuslGraph,
	}
	if !opts.ConfineFinGrain {
		argv = append(argv, "--no-fine-grain")
	}
	if s := strings.Join(opts.LdPaths, ":"); s != "" {
		argv = append(argv, "--ld-paths="+s)
	}
	if opts.Sysroot != "" {
		argv = append(argv, "--sysroot="+opts.Sysroot)
	}
	argv = append(argv, binaryPath)

	cmd := exec.CommandContext(ctx, argv[0], argv[1:]...)
	var stdout, stderr bytes.Buffer
	cmd.Stdout = &stdout
	cmd.Stderr = &stderr

	if err := cmd.Run(); err != nil {
		msg := strings.TrimSpace(stderr.String())
		if msg == "" {
			msg = err.Error()
		}
		return nil, fmt.Errorf("%s", msg)
	}

	var out []uint16
	sc := bufio.NewScanner(&stdout)
	for sc.Scan() {
		line := strings.TrimSpace(sc.Text())
		if line == "" {
			continue
		}
		n, err := strconv.ParseUint(line, 10, 16)
		if err != nil {
			return nil, fmt.Errorf("unexpected output line %q: %v", line, err)
		}
		out = append(out, uint16(n))
	}
	return out, sc.Err()
}
