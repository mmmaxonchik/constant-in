package analysis

import (
	"bufio"
	"bytes"
	"context"
	"fmt"
	"os"
	"os/exec"
	"strconv"
	"strings"
)

const (
	sysfilterHelperPath = "/opt/tools/bin/constant-in-sysfilter-helper"
	syspartHelperPath   = "/opt/tools/bin/constant-in-syspart-helper"
)

type sysfilterAnalyzer struct{}

func NewSysfilterAnalyzer() StaticAnalyzer { return &sysfilterAnalyzer{} }

func (*sysfilterAnalyzer) Name() string { return "sysfilter" }

func (*sysfilterAnalyzer) Analyze(ctx context.Context, binaryPath string, opts StaticOptions) ([]uint16, error) {
	return runEgalitoHelper(ctx, "sysfilter", binaryPath, opts)
}

type syspartAnalyzer struct{}

func NewSyspartAnalyzer() StaticAnalyzer { return &syspartAnalyzer{} }

func (*syspartAnalyzer) Name() string { return "syspart" }

func (*syspartAnalyzer) Analyze(ctx context.Context, binaryPath string, opts StaticOptions) ([]uint16, error) {
	return runEgalitoHelper(ctx, "syspart", binaryPath, opts)
}

func runEgalitoHelper(ctx context.Context, tool, binaryPath string, opts StaticOptions) ([]uint16, error) {
	helper, libraryPath := egalitoHelper(tool)
	if envHelper := os.Getenv("CONSTANT_IN_EGALITO_HELPER"); envHelper != "" {
		helper = envHelper
	}

	argv := []string{
		"--binary=" + binaryPath,
	}
	if s := strings.Join(opts.LdPaths, ":"); s != "" {
		argv = append(argv, "--ld-paths="+s)
	}
	if opts.Sysroot != "" {
		argv = append(argv, "--sysroot="+opts.Sysroot)
	}
	if tool == "syspart" && opts.SyspartIcAnalysis {
		argv = append(argv, "--syspart-icanalysis")
	}

	cmd := exec.CommandContext(ctx, helper, argv...)
	cmd.Env = egalitoHelperEnv(os.Environ(), libraryPath)
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
			return nil, fmt.Errorf("%s: unexpected helper output line %q: %v", tool, line, err)
		}
		out = append(out, uint16(n))
	}
	if err := sc.Err(); err != nil {
		return nil, err
	}
	return out, nil
}

func egalitoHelper(tool string) (string, string) {
	switch tool {
	case "sysfilter":
		return sysfilterHelperPath, "/opt/sysfilter/lib"
	case "syspart":
		return syspartHelperPath, "/opt/syspart_new/lib"
	default:
		return "", ""
	}
}

func egalitoHelperEnv(base []string, libraryPath string) []string {
	env := make([]string, 0, len(base)+1)
	for _, kv := range base {
		if strings.HasPrefix(kv, "LD_LIBRARY_PATH=") {
			continue
		}
		env = append(env, kv)
	}
	if libraryPath != "" {
		env = append(env, "LD_LIBRARY_PATH="+libraryPath)
	}
	return env
}
