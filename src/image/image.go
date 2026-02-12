package image

import (
	"context"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"strings"

	"github.com/containerd/containerd/v2/pkg/archive"
	"github.com/google/go-containerregistry/pkg/authn"
	"github.com/google/go-containerregistry/pkg/crane"
	v1 "github.com/google/go-containerregistry/pkg/v1"
	"mvdan.cc/sh/v3/syntax"

	"github.com/mmmaxonchik/constant-in/src/logger"
)

type ImageInfo struct {
	RootfsDir string

	Executables []string

	cleanup func() error
}

func (i *ImageInfo) Close() error {
	if i.cleanup != nil {
		return i.cleanup()
	}
	return nil
}

type LoadOptions struct {
	EntrypointOverride *[]string

	CmdOverride *[]string
}

func Load(ctx context.Context, imageRef string, log logger.Logger, opts LoadOptions) (*ImageInfo, error) {
	log.Infof("pulling image %s", imageRef)

	img, err := crane.Pull(imageRef,
		crane.WithContext(ctx),
		crane.WithAuthFromKeychain(authn.DefaultKeychain),
	)
	if err != nil {
		return nil, fmt.Errorf("pull %q: %w", imageRef, err)
	}

	tmpDir, err := os.MkdirTemp("", "constantin-rootfs-*")
	if err != nil {
		return nil, fmt.Errorf("create temp dir: %w", err)
	}

	log.Debugf("extracting rootfs to %s", tmpDir)
	if err := extractRootfs(ctx, img, tmpDir, log); err != nil {
		os.RemoveAll(tmpDir)
		return nil, fmt.Errorf("extract rootfs: %w", err)
	}

	tmpDir = tmpDir + "/rootfs"

	cfg, err := img.ConfigFile()
	if err != nil {
		os.RemoveAll(tmpDir)
		return nil, fmt.Errorf("read image config: %w", err)
	}

	entrypoint := cfg.Config.Entrypoint
	if opts.EntrypointOverride != nil {
		entrypoint = *opts.EntrypointOverride
	}
	cmd := cfg.Config.Cmd
	if opts.CmdOverride != nil {
		cmd = *opts.CmdOverride
	}

	execs := discoverExecutables(entrypoint, cmd, cfg.Config.Env, tmpDir)
	log.Debugf("discovered %d executable(s) from ENTRYPOINT/CMD", len(execs))

	return &ImageInfo{
		RootfsDir:   tmpDir,
		Executables: execs,
		cleanup:     func() error { return os.RemoveAll(tmpDir) },
	}, nil
}

func extractRootfs(ctx context.Context, img v1.Image, destDir string, log logger.Logger) error {
	rootfsDir := filepath.Join(destDir, "rootfs")
	if err := os.MkdirAll(rootfsDir, 0755); err != nil {
		return fmt.Errorf("create rootfs dir: %w", err)
	}

	layers, err := img.Layers()
	if err != nil {
		return fmt.Errorf("read image layers: %w", err)
	}
	log.Debugf("applying %d image layer(s)", len(layers))

	for idx, l := range layers {
		r, err := l.Uncompressed()
		if err != nil {
			return fmt.Errorf("open layer %d: %w", idx, err)
		}
		if _, err := archive.Apply(ctx, rootfsDir, r); err != nil {
			r.Close()
			return fmt.Errorf("apply layer %d: %w", idx, err)
		}
		if err := r.Close(); err != nil {
			return fmt.Errorf("close layer %d: %w", idx, err)
		}
	}
	return nil
}

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

func resolveInRootfs(p, rootfsDir string, maxDepth int) string {
	for range maxDepth {
		info, err := os.Lstat(p)
		if err != nil || info.Mode()&os.ModeSymlink == 0 {
			return p
		}
		link, err := os.Readlink(p)
		if err != nil {
			return p
		}
		if filepath.IsAbs(link) {
			p = filepath.Join(rootfsDir, link)
		} else {
			p = filepath.Join(filepath.Dir(p), link)
		}
	}
	return p
}

func (info *ImageInfo) ResolveExecutablesFromPaths(paths []string, log logger.Logger) []string {
	seen := make(map[string]bool)
	var out []string
	for _, p := range paths {
		if p == "" || p[0] != '/' {
			log.Debugf("resolve %s: skipped (not absolute)", p)
			continue
		}
		full := filepath.Join(info.RootfsDir, p)
		real := resolveInRootfs(full, info.RootfsDir, 10)
		if seen[real] {
			log.Debugf("resolve %s: skipped (duplicate of already-seen path %s)", p, real)
			continue
		}
		fi, err := os.Lstat(real)
		if err != nil {
			log.Debugf("resolve %s: skipped (not found in rootfs: %v)", p, err)
			continue
		}
		if fi.IsDir() {
			log.Debugf("resolve %s: skipped (is a directory)", p)
			continue
		}
		if !isELF(real) {
			log.Debugf("resolve %s: skipped (not an ELF binary)", p)
			continue
		}
		seen[real] = true
		out = append(out, real)
	}
	return out
}

var knownShells = map[string]bool{
	"sh": true, "bash": true, "zsh": true, "ash": true, "dash": true, "busybox": true,
}

func isShellCPattern(tokens []string) bool {
	return len(tokens) >= 3 &&
		tokens[1] == "-c" &&
		knownShells[filepath.Base(tokens[0])]
}

//

//

func discoverExecutables(entrypoint, cmd, env []string, rootfsDir string) []string {
	var effective []string
	if len(entrypoint) > 0 {
		effective = append(entrypoint, cmd...)
	} else {
		effective = cmd
	}
	if len(effective) == 0 {
		return nil
	}

	var cmdNames []string
	if isShellCPattern(effective) {

		cmdNames = append(cmdNames, effective[0])

		cmdNames = append(cmdNames, parseCommandNames(effective[2])...)
	} else {

		parts := make([]string, 0, len(effective))
		for _, tok := range effective {
			q, err := syntax.Quote(tok, syntax.LangPOSIX)
			if err != nil {
				q = tok
			}
			parts = append(parts, q)
		}
		cmdNames = parseCommandNames(strings.Join(parts, " "))
	}

	pathDirs := pathDirsInRootfs(env, rootfsDir)

	seen := make(map[string]bool)
	var out []string
	for _, name := range cmdNames {
		p := resolveCommandInRootfs(name, pathDirs, rootfsDir)
		if p == "" {
			continue
		}
		real := resolveInRootfs(p, rootfsDir, 10)
		if seen[real] {
			continue
		}
		info, err := os.Lstat(real)
		if err != nil || info.IsDir() {
			continue
		}
		if !isELF(real) {
			continue
		}
		seen[real] = true
		out = append(out, real)
	}
	return out
}

func parseCommandNames(src string) []string {
	f, err := syntax.NewParser().Parse(strings.NewReader(src), "")
	if err != nil {

		if fields := strings.Fields(src); len(fields) > 0 {
			return []string{fields[0]}
		}
		return nil
	}
	seen := make(map[string]bool)
	var names []string
	syntax.Walk(f, func(n syntax.Node) bool {
		call, ok := n.(*syntax.CallExpr)
		if !ok || len(call.Args) == 0 {
			return true
		}
		w := call.Args[0]
		if len(w.Parts) == 1 {
			if lit, ok := w.Parts[0].(*syntax.Lit); ok && !seen[lit.Value] {
				seen[lit.Value] = true
				names = append(names, lit.Value)
			}
		}
		return true
	})
	return names
}

func pathDirsInRootfs(env []string, rootfsDir string) []string {
	for _, e := range env {
		if v, ok := strings.CutPrefix(e, "PATH="); ok && v != "" {
			var dirs []string
			for _, d := range strings.Split(v, ":") {
				if d != "" {
					dirs = append(dirs, filepath.Join(rootfsDir, d))
				}
			}
			return dirs
		}
	}
	fallback := []string{
		"/usr/local/sbin", "/usr/local/bin",
		"/usr/sbin", "/usr/bin",
		"/sbin", "/bin",
	}
	dirs := make([]string, len(fallback))
	for i, d := range fallback {
		dirs[i] = filepath.Join(rootfsDir, d)
	}
	return dirs
}

func resolveCommandInRootfs(name string, pathDirs []string, rootfsDir string) string {
	if filepath.IsAbs(name) {
		return filepath.Join(rootfsDir, name)
	}
	for _, d := range pathDirs {
		p := filepath.Join(d, name)
		if _, err := os.Lstat(p); err == nil {
			return p
		}
	}
	return ""
}
