package main

import (
	"context"
	"encoding/json"
	"fmt"
	"os"
	"path/filepath"
	"strings"

	"github.com/mmmaxonchik/constant-in/src/logger"
	"github.com/mmmaxonchik/constant-in/src/pipeline"
)

func main() {
	cfg := pipeline.DefaultConfig()
	logLevel := "info"
	outputPath := "result.json"
	seccompOutputPath := ""
	seccompPerInstrument := false
	seccompDefaultSyscalls := pipeline.DefaultDockerSyscalls

	var entrypointTokens []string
	entrypointSet := false
	var cmdTokens []string
	cmdSet := false

	args := os.Args[1:]
	for len(args) > 0 {
		arg := args[0]
		switch {
		case strings.HasPrefix(arg, "--mode="):
			cfg.Mode = pipeline.Mode(strings.TrimPrefix(arg, "--mode="))
		case strings.HasPrefix(arg, "--image="):
			cfg.ImageRef = strings.TrimPrefix(arg, "--image=")
		case strings.HasPrefix(arg, "--container="):
			cfg.ContainerID = strings.TrimPrefix(arg, "--container=")
		case strings.HasPrefix(arg, "--sysroot="):
			cfg.Sysroot = strings.TrimPrefix(arg, "--sysroot=")
		case strings.HasPrefix(arg, "--ld-paths="):
			cfg.LdPaths = strings.Split(strings.TrimPrefix(arg, "--ld-paths="), ":")
		case strings.HasPrefix(arg, "--static-tools="):
			raw := strings.TrimPrefix(arg, "--static-tools=")
			valid := map[string]bool{"sysfilter": true, "syspart": true, "confine": true, "go2seccomp": true}
			for _, t := range strings.Split(raw, ",") {
				t = strings.TrimSpace(t)
				if !valid[t] {
					fmt.Fprintf(os.Stderr, "unknown static tool %q (valid: sysfilter, syspart, confine, go2seccomp)\n", t)
					os.Exit(1)
				}
				cfg.Tools = append(cfg.Tools, t)
			}
		case arg == "--auto-choice":
			cfg.AutoChoice = true
		case arg == "--confine-fine-grain=false":
			cfg.ConfineFinGrain = false
		case arg == "--syspart-icanalysis":
			cfg.SyspartIcAnalysis = true
		case strings.HasPrefix(arg, "--dynamic-tools="):
			raw := strings.TrimPrefix(arg, "--dynamic-tools=")
			validDyn := map[string]bool{"tracee": true}
			for _, t := range strings.Split(raw, ",") {
				t = strings.TrimSpace(t)
				if !validDyn[t] {
					fmt.Fprintf(os.Stderr, "unknown dynamic tool %q (valid: tracee)\n", t)
					os.Exit(1)
				}
				cfg.DynamicTools = append(cfg.DynamicTools, t)
			}
		case strings.HasPrefix(arg, "--tracee-log="):
			cfg.TraceeLogPath = strings.TrimPrefix(arg, "--tracee-log=")
		case strings.HasPrefix(arg, "--entrypoint="):
			raw := strings.TrimPrefix(arg, "--entrypoint=")
			if err := json.Unmarshal([]byte(raw), &entrypointTokens); err != nil {
				fmt.Fprintf(os.Stderr, "--entrypoint: invalid JSON array %q: %v\n", raw, err)
				os.Exit(1)
			}
			entrypointSet = true
		case strings.HasPrefix(arg, "--cmd="):
			raw := strings.TrimPrefix(arg, "--cmd=")
			if err := json.Unmarshal([]byte(raw), &cmdTokens); err != nil {
				fmt.Fprintf(os.Stderr, "--cmd: invalid JSON array %q: %v\n", raw, err)
				os.Exit(1)
			}
			cmdSet = true
		case strings.HasPrefix(arg, "--output="):
			outputPath = strings.TrimPrefix(arg, "--output=")
		case strings.HasPrefix(arg, "--seccomp-output="):
			seccompOutputPath = strings.TrimPrefix(arg, "--seccomp-output=")
		case arg == "--seccomp-per-instrument=true":
			seccompPerInstrument = true
		case arg == "--seccomp-per-instrument=false":
			seccompPerInstrument = false
		case strings.HasPrefix(arg, "--seccomp-default-syscalls="):
			raw := strings.TrimPrefix(arg, "--seccomp-default-syscalls=")
			seccompDefaultSyscalls = strings.Split(raw, ",")
		case strings.HasPrefix(arg, "--log-level="):
			logLevel = strings.TrimPrefix(arg, "--log-level=")
		case arg == "--help", arg == "-h":
			printUsage()
			os.Exit(0)
		default:
			fmt.Fprintf(os.Stderr, "unknown flag: %s\n\n", arg)
			printUsage()
			os.Exit(1)
		}
		args = args[1:]
	}

	if entrypointSet {
		cfg.EntrypointOverride = &entrypointTokens
	}
	if cmdSet {
		cfg.CmdOverride = &cmdTokens
	}

	if cfg.Mode == "" {
		fmt.Fprintln(os.Stderr, "error: --mode is required\n")
		printUsage()
		os.Exit(1)
	}

	log := logger.NewFromString(logLevel)

	result, err := pipeline.Run(context.Background(), cfg, log)
	if err != nil {
		log.Fatalf("%v", err)
	}

	data, err := json.MarshalIndent(result, "", "  ")
	if err != nil {
		log.Fatalf("marshal result: %v", err)
	}
	if err := os.WriteFile(outputPath, data, 0644); err != nil {
		log.Fatalf("write result to %s: %v", outputPath, err)
	}
	log.Infof("results written to %s", outputPath)

	if seccompOutputPath != "" {
		profile, err := pipeline.BuildSeccompProfile(result, seccompDefaultSyscalls)
		if err != nil {
			log.Fatalf("build seccomp profile: %v", err)
		}
		seccompData, err := pipeline.MarshalSeccompProfile(profile)
		if err != nil {
			log.Fatalf("marshal seccomp profile: %v", err)
		}
		if err := os.WriteFile(seccompOutputPath, seccompData, 0644); err != nil {
			log.Fatalf("write seccomp profile to %s: %v", seccompOutputPath, err)
		}
		log.Infof("seccomp profile written to %s (%d syscalls allowed)", seccompOutputPath, len(profile.Syscalls[0].Names))

		if seccompPerInstrument {
			dir := filepath.Dir(seccompOutputPath)
			base := filepath.Base(seccompOutputPath)
			for _, tool := range pipeline.AllToolNames(result) {
				toolProfile, err := pipeline.BuildSeccompProfileForTool(result, tool, seccompDefaultSyscalls)
				if err != nil {
					log.Fatalf("build seccomp profile for tool %s: %v", tool, err)
				}
				toolData, err := pipeline.MarshalSeccompProfile(toolProfile)
				if err != nil {
					log.Fatalf("marshal seccomp profile for tool %s: %v", tool, err)
				}
				toolPath := filepath.Join(dir, tool+"_"+base)
				if err := os.WriteFile(toolPath, toolData, 0644); err != nil {
					log.Fatalf("write seccomp profile to %s: %v", toolPath, err)
				}
				log.Infof("seccomp profile for %s written to %s (%d syscalls allowed)", tool, toolPath, len(toolProfile.Syscalls[0].Names))
			}
		}
	}
}

func printUsage() {
	fmt.Fprint(os.Stderr, `Usage: constnt-in [flags]

Flags:
  --mode=image|container|both
        Analysis mode (required).
        image:     pull OCI image, run static analysers on ENTRYPOINT/CMD binaries.
        container: run dynamic analysers for an already-running container.
        both:      fetch image ref from runtime, run static + dynamic analysis.

  --image=<ref>
        OCI image reference, e.g. nginx:latest or registry.io/org/app:v1.2
        Required for --mode=image; optional for --mode=both (overrides runtime ref).

  --container=<id>
        Container ID. Required for --mode=container and --mode=both.

  --static-tools=sysfilter,syspart,confine,go2seccomp
        Comma-separated list of static tools to run (default: all three classic tools).
        go2seccomp is a native analyzer for statically-linked Go binaries.
        At least one required. Applicable to image and both modes.

  --auto-choice
        Enable smart per-binary tool selection.
        go2seccomp is applied only to binaries detected as Go executables.
        In default (all-tools) mode, go2seccomp is added automatically and
        skipped for non-Go binaries. Without this flag every configured tool
        runs on every ELF binary.

  --syspart-icanalysis
        Enable indirect-call analysis in syspart (default: off).
        Resolves call-through-pointer targets and adds edges to all
        address-taken functions, increasing syscall coverage.
        Warning: may cause OOM on large binaries linked with libssl/libcrypto.

  --confine-fine-grain=false
        Disable fine-grain .so analysis in confine (enabled by default).
        In fine-grain mode, .so files with a dedicated callgraph in
        other-callgraphs/ are analysed as:
          imported_func → lib-callgraph → libc-func → syscall(N).
        Disabling falls back to plain objdump for all .so dependencies.

  --dynamic-tools=tracee
        Comma-separated list of dynamic tools to run (default: tracee).
        At least one required for container and both modes.

  --tracee-log=<file>
        Path to a tracee NDJSON event log.
        Required when "tracee" is in --dynamic-tools.

  --entrypoint=<json-array>
        Override the image ENTRYPOINT for executable discovery (image/both modes).
        Must be a JSON string array matching the OCI exec form.
        Example: --entrypoint='["sh", "-c", "redis-server"]'

  --cmd=<json-array>
        Override the image CMD for executable discovery.
        Must be a JSON string array matching the OCI exec form.
        Example: --cmd='["nginx", "-g", "daemon off;"]'

  --sysroot=<path>
        Override the sysroot passed to static tools.
        Defaults to the unpacked image rootfs.

  --ld-paths=<path:path>
        Colon-separated extra library search paths appended to rootfs defaults.

  --output=<file>
        Path to write the JSON result (default: result.json).

  --seccomp-output=<file>
        Optional. Path to write a Docker seccomp profile (JSON) derived from
        the combined syscall results. Uses deny-by-default (SCMP_ACT_ERRNO)
        and allows exactly the syscalls observed across all tools and binaries.

  --seccomp-per-instrument=true|false
        When true, also write a separate seccomp profile for each tool.
        Each file is named <tool>_<seccomp-output-file> in the same directory.
        Default: false.

  --seccomp-default-syscalls=<name,name,...>
        Comma-separated list of syscall names always included in every seccomp
        profile (combined and per-instrument). Intended for syscalls required
        by Docker to create a container. Defaults to a built-in baseline set.

  --log-level=trace|debug|info|warn|error
        Log verbosity (default: info). Logs go to stderr, results to --output file.
`)
}
