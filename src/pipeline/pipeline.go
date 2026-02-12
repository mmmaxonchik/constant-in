//

package pipeline

import (
	"context"
	"fmt"
	"os"
	"path/filepath"
	"sort"

	"github.com/mmmaxonchik/constant-in/src/analysis"
	"github.com/mmmaxonchik/constant-in/src/image"
	"github.com/mmmaxonchik/constant-in/src/logger"
	cruntime "github.com/mmmaxonchik/constant-in/src/runtime"
)

type Mode string

const (
	ModeImage     Mode = "image"
	ModeContainer Mode = "container"
	ModeBoth      Mode = "both"
)

type Config struct {
	Mode        Mode
	ImageRef    string
	ContainerID string
	Sysroot     string
	LdPaths     []string

	Tools []string

	AutoChoice bool

	ConfineFinGrain bool

	SyspartIcAnalysis bool

	DynamicTools []string

	TraceeLogPath string

	EntrypointOverride *[]string

	CmdOverride *[]string

	RuntimeSockets cruntime.RuntimeSockets
}

func DefaultConfig() Config {
	return Config{
		RuntimeSockets:  cruntime.DefaultRuntimeSockets(),
		ConfineFinGrain: true,
	}
}

func Run(ctx context.Context, cfg Config, log logger.Logger) (*Result, error) {
	switch cfg.Mode {
	case ModeImage:
		if cfg.ImageRef == "" {
			return nil, fmt.Errorf("--image is required for image mode")
		}
		return runImage(ctx, cfg, buildStaticAnalyzers(cfg), log)

	case ModeContainer:
		if cfg.ContainerID == "" {
			return nil, fmt.Errorf("--container is required for container mode")
		}
		dyn, err := buildDynamicAnalyzers(cfg)
		if err != nil {
			return nil, err
		}
		return runContainer(ctx, cfg, dyn, log)

	case ModeBoth:
		if cfg.ContainerID == "" {
			return nil, fmt.Errorf("--container is required for both mode")
		}
		dyn, err := buildDynamicAnalyzers(cfg)
		if err != nil {
			return nil, err
		}
		return runBoth(ctx, cfg, buildStaticAnalyzers(cfg), dyn, log)

	default:
		return nil, fmt.Errorf("unknown mode %q (valid: image, container, both)", cfg.Mode)
	}
}

func runImage(ctx context.Context, cfg Config, analyzers []analysis.StaticAnalyzer, log logger.Logger) (*Result, error) {
	img, err := image.Load(ctx, cfg.ImageRef, log, image.LoadOptions{
		EntrypointOverride: cfg.EntrypointOverride,
		CmdOverride:        cfg.CmdOverride,
	})
	if err != nil {
		return nil, fmt.Errorf("load image: %w", err)
	}
	defer img.Close()

	log.Infof("rootfs at %s, %d executable(s) from entrypoint/cmd", img.RootfsDir, len(img.Executables))
	logEntrypointBinaries(img.RootfsDir, img.Executables, log)
	staticResults := runStaticOnBinaries(ctx, img, cfg, img.Executables, analyzers, log)
	staticEntries := buildStaticEntries(img.RootfsDir, staticResults)

	return &Result{
		Mode:     "image",
		Image:    cfg.ImageRef,
		Static:   staticEntries,
		Combined: combinedEntries(staticEntries, nil),
		PerTool:  buildPerToolSyscalls(staticEntries, nil),
	}, nil
}

func runContainer(ctx context.Context, cfg Config, analyzers []analysis.DynamicAnalyzer, log logger.Logger) (*Result, error) {
	dynResult, err := analysis.RunDynamic(ctx, analyzers, analysis.DynamicOptions{
		ContainerID: cfg.ContainerID,
	}, log)
	if err != nil {
		return nil, err
	}
	dynEntries := buildDynamicEntries(dynResult)
	return &Result{
		Mode:     "container",
		Dynamic:  dynEntries,
		Combined: combinedEntries(nil, dynEntries),
		PerTool:  buildPerToolSyscalls(nil, dynResult),
	}, nil
}

func runBoth(
	ctx context.Context,
	cfg Config,
	staticAnalyzers []analysis.StaticAnalyzer,
	dynAnalyzers []analysis.DynamicAnalyzer,
	log logger.Logger,
) (*Result, error) {

	log.Infof("inspecting container %s", cfg.ContainerID)
	inspected, err := cruntime.InspectContainerAllRuntimes(ctx, cfg.ContainerID, cfg.RuntimeSockets)
	if err != nil {
		return nil, fmt.Errorf("inspect container: %w", err)
	}
	if inspected.Info == nil {
		return nil, fmt.Errorf("container %q not found in any runtime", cfg.ContainerID)
	}
	for _, e := range inspected.Errors {
		log.Debugf("runtime probe: %s", e)
	}

	imageRef := cfg.ImageRef
	if imageRef == "" {
		imageRef = inspected.Info.ImageRef
	}
	if imageRef == "" {
		return nil, fmt.Errorf("could not determine image reference from container runtime")
	}
	log.Infof("using image %s", imageRef)

	dynResult, err := analysis.RunDynamic(ctx, dynAnalyzers, analysis.DynamicOptions{
		ContainerID: cfg.ContainerID,
	}, log)
	if err != nil {
		return nil, fmt.Errorf("dynamic analysis: %w", err)
	}

	runtimeEP := inspected.Info.Entrypoint
	ep := &runtimeEP
	if cfg.EntrypointOverride != nil {
		ep = cfg.EntrypointOverride
	}
	runtimeCmd := inspected.Info.Cmd
	cmd := &runtimeCmd
	if cfg.CmdOverride != nil {
		cmd = cfg.CmdOverride
	}

	img, err := image.Load(ctx, imageRef, log, image.LoadOptions{
		EntrypointOverride: ep,
		CmdOverride:        cmd,
	})
	if err != nil {
		return nil, fmt.Errorf("load image: %w", err)
	}
	defer img.Close()

	log.Infof("%d executable(s) from entrypoint/cmd", len(img.Executables))
	logEntrypointBinaries(img.RootfsDir, img.Executables, log)
	dynPaths := collectDynExecPaths(dynResult)
	dynBinaries := img.ResolveExecutablesFromPaths(dynPaths, log)
	log.Infof("dynamic analysis found %d resolvable ELF binaries in rootfs", len(dynBinaries))

	allBinaries := mergePaths(img.Executables, dynBinaries)
	log.Infof("total binaries for static analysis: %d", len(allBinaries))

	staticResults := runStaticOnBinaries(ctx, img, cfg, allBinaries, staticAnalyzers, log)

	staticEntries := buildStaticEntries(img.RootfsDir, staticResults)
	dynEntries := buildDynamicEntries(dynResult)

	return &Result{
		Mode:     "both",
		Image:    imageRef,
		Static:   staticEntries,
		Dynamic:  dynEntries,
		Combined: combinedEntries(staticEntries, dynEntries),
		PerTool:  buildPerToolSyscalls(staticEntries, dynResult),
	}, nil
}

func buildPerToolSyscalls(static []BinaryEntry, dynResult *analysis.DynamicResult) map[string][]uint16 {
	sets := make(map[string]map[uint16]struct{})

	for _, entry := range static {
		for tool, sc := range entry.Syscalls {
			if sets[tool] == nil {
				sets[tool] = make(map[uint16]struct{})
			}
			for _, n := range sc {
				sets[tool][n] = struct{}{}
			}
		}
	}

	if dynResult != nil {
		for toolName, toolResult := range dynResult.PerTool {
			if sets[toolName] == nil {
				sets[toolName] = make(map[uint16]struct{})
			}
			for _, n := range toolResult.Syscalls {
				sets[toolName][n] = struct{}{}
			}
		}
	}

	out := make(map[string][]uint16, len(sets))
	for tool, numSet := range sets {
		nums := make([]uint16, 0, len(numSet))
		for n := range numSet {
			nums = append(nums, n)
		}
		sort.Slice(nums, func(i, j int) bool { return nums[i] < nums[j] })
		out[tool] = nums
	}
	return out
}

func runStaticOnBinaries(
	ctx context.Context,
	img *image.ImageInfo,
	cfg Config,
	binaries []string,
	analyzers []analysis.StaticAnalyzer,
	log logger.Logger,
) []analysis.BinaryResult {
	sysroot := img.RootfsDir
	if cfg.Sysroot != "" {
		sysroot = cfg.Sysroot
	}
	ldPaths := append(defaultLdPaths(img.RootfsDir), cfg.LdPaths...)
	opts := analysis.StaticOptions{
		Sysroot:           sysroot,
		LdPaths:           ldPaths,
		ConfineFinGrain:   cfg.ConfineFinGrain,
		SyspartIcAnalysis: cfg.SyspartIcAnalysis,
	}

	go2s, classic := splitAnalyzers(analyzers)

	var results []analysis.BinaryResult
	for _, execPath := range binaries {
		if rel, err := filepath.Rel(img.RootfsDir, execPath); err == nil {
			log.Infof("analysing /%s", rel)
		} else {
			log.Infof("analysing %s", execPath)
		}
		br, err := runStaticSingle(ctx, execPath, go2s, classic, cfg.AutoChoice, opts, log)
		if err != nil {
			log.Warnf("skip %s: %v", execPath, err)
			continue
		}
		if rel, err := filepath.Rel(img.RootfsDir, execPath); err == nil {
			br.Path = "/" + rel
		}
		results = append(results, *br)
	}

	toolSets := make(map[string]map[uint16]struct{})
	for _, br := range results {
		for tool, sc := range br.PerTool {
			if toolSets[tool] == nil {
				toolSets[tool] = make(map[uint16]struct{})
			}
			for _, n := range sc {
				toolSets[tool][n] = struct{}{}
			}
		}
	}
	toolNames := make([]string, 0, len(toolSets))
	for t := range toolSets {
		toolNames = append(toolNames, t)
	}
	sort.Strings(toolNames)
	for _, t := range toolNames {
		log.Infof("%s: %d unique syscalls total across all binaries", t, len(toolSets[t]))
	}

	log.Infof("static analysis complete: %d binary(ies) analysed", len(results))
	return results
}

func logEntrypointBinaries(rootfsDir string, binaries []string, log logger.Logger) {
	for _, p := range binaries {
		if rel, err := filepath.Rel(rootfsDir, p); err == nil {
			log.Infof("  entrypoint binary: /%s", rel)
		}
	}
}

func runStaticSingle(
	ctx context.Context,
	execPath string,
	go2s analysis.StaticAnalyzer,
	classic []analysis.StaticAnalyzer,
	autoChoice bool,
	opts analysis.StaticOptions,
	log logger.Logger,
) (*analysis.BinaryResult, error) {
	if !autoChoice {
		all := classic
		if go2s != nil {
			all = append([]analysis.StaticAnalyzer{go2s}, classic...)
		}
		return analysis.RunStatic(ctx, execPath, all, opts, log)
	}

	if go2s != nil && analysis.IsGoBinary(execPath) {
		br, err := analysis.RunStatic(ctx, execPath, []analysis.StaticAnalyzer{go2s}, opts, log)
		if err == nil && br != nil && len(br.Combined) > 0 {
			return br, nil
		}
		base := filepath.Base(execPath)
		if err != nil {
			log.Debugf("go2seccomp failed for %s (%v), falling back to classic tools", base, err)
		} else {
			log.Debugf("go2seccomp returned no syscalls for %s, falling back to classic tools", base)
		}
	}

	return analysis.RunStatic(ctx, execPath, classic, opts, log)
}

func buildStaticEntries(rootfsDir string, results []analysis.BinaryResult) []BinaryEntry {
	_ = rootfsDir
	entries := make([]BinaryEntry, 0, len(results))
	for _, br := range results {
		e := BinaryEntry{Path: br.Path, Syscalls: make(map[string][]uint16, len(br.PerTool))}
		for tool, sc := range br.PerTool {
			if len(sc) > 0 {
				e.Syscalls[tool] = sc
			}
		}
		entries = append(entries, e)
	}
	return entries
}

func buildDynamicEntries(dynResult *analysis.DynamicResult) []BinaryEntry {
	if dynResult == nil {
		return nil
	}
	byPath := make(map[string]*BinaryEntry)
	for toolName, toolResult := range dynResult.PerTool {
		for execPath, syscalls := range toolResult.PerExec {
			if len(syscalls) == 0 {
				continue
			}
			e, ok := byPath[execPath]
			if !ok {
				entry := BinaryEntry{Path: execPath, Syscalls: make(map[string][]uint16)}
				byPath[execPath] = &entry
				e = &entry
			}
			e.Syscalls[toolName] = syscalls
		}
	}
	entries := make([]BinaryEntry, 0, len(byPath))
	for _, e := range byPath {
		entries = append(entries, *e)
	}
	sort.Slice(entries, func(i, j int) bool { return entries[i].Path < entries[j].Path })
	return entries
}

func combinedEntries(static, dynamic []BinaryEntry) []CombinedEntry {
	byPath := make(map[string]map[uint16]struct{})
	for _, e := range append(static, dynamic...) {
		if _, ok := byPath[e.Path]; !ok {
			byPath[e.Path] = make(map[uint16]struct{})
		}
		for _, sc := range e.Syscalls {
			for _, num := range sc {
				byPath[e.Path][num] = struct{}{}
			}
		}
	}
	entries := make([]CombinedEntry, 0, len(byPath))
	for path, scSet := range byPath {
		nums := make([]uint16, 0, len(scSet))
		for n := range scSet {
			nums = append(nums, n)
		}
		sort.Slice(nums, func(i, j int) bool { return nums[i] < nums[j] })
		entries = append(entries, CombinedEntry{Path: path, Syscalls: nums})
	}
	sort.Slice(entries, func(i, j int) bool { return entries[i].Path < entries[j].Path })
	return entries
}

func collectDynExecPaths(dynResult *analysis.DynamicResult) []string {
	if dynResult == nil {
		return nil
	}
	seen := make(map[string]bool)
	var out []string
	for _, tr := range dynResult.PerTool {
		for p := range tr.PerExec {
			if !seen[p] {
				seen[p] = true
				out = append(out, p)
			}
		}
	}
	return out
}

func mergePaths(a, b []string) []string {
	seen := make(map[string]bool, len(a)+len(b))
	var out []string
	for _, p := range append(a, b...) {
		if !seen[p] {
			seen[p] = true
			out = append(out, p)
		}
	}
	return out
}

func splitAnalyzers(analyzers []analysis.StaticAnalyzer) (go2s analysis.StaticAnalyzer, classic []analysis.StaticAnalyzer) {
	for _, a := range analyzers {
		if a.Name() == "go2seccomp" {
			go2s = a
		} else {
			classic = append(classic, a)
		}
	}
	return
}

func buildStaticAnalyzers(cfg Config) []analysis.StaticAnalyzer {
	tools := cfg.Tools
	if len(tools) == 0 {
		tools = []string{"sysfilter", "syspart", "confine"}
		if cfg.AutoChoice {

			tools = append(tools, "go2seccomp")
		}
	}
	out := make([]analysis.StaticAnalyzer, 0, len(tools))
	for _, name := range tools {
		switch name {
		case "sysfilter":
			out = append(out, analysis.NewSysfilterAnalyzer())
		case "syspart":
			out = append(out, analysis.NewSyspartAnalyzer())
		case "confine":
			out = append(out, analysis.NewConfineAnalyzer())
		case "go2seccomp":
			out = append(out, analysis.NewGo2SeccompAnalyzer())
		}
	}
	return out
}

func buildDynamicAnalyzers(cfg Config) ([]analysis.DynamicAnalyzer, error) {
	tools := cfg.DynamicTools
	if len(tools) == 0 {
		tools = []string{"tracee"}
	}
	out := make([]analysis.DynamicAnalyzer, 0, len(tools))
	for _, name := range tools {
		switch name {
		case "tracee":
			if cfg.TraceeLogPath == "" {
				return nil, fmt.Errorf("--tracee-log is required when using the tracee dynamic analyzer")
			}
			out = append(out, analysis.NewTraceeAnalyzer(cfg.TraceeLogPath))
		default:
			return nil, fmt.Errorf("unknown dynamic tool %q (valid: tracee)", name)
		}
	}
	return out, nil
}

func defaultLdPaths(rootfsDir string) []string {
	candidates := []string{
		"/lib", "/usr/lib",
		"/lib/x86_64-linux-gnu", "/usr/lib/x86_64-linux-gnu",
		"/lib64", "/usr/lib64",
	}
	var out []string
	for _, c := range candidates {
		p := filepath.Join(rootfsDir, c)
		if _, err := os.Stat(p); err == nil {
			out = append(out, p)
		}
	}
	return out
}
