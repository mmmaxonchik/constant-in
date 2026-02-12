package runtime

import (
	"context"
	"encoding/json"
	"fmt"
	"net"
	"net/http"
	"os"
	"strings"
	"time"

	containerd "github.com/containerd/containerd/v2/client"
	"github.com/containerd/containerd/v2/pkg/namespaces"
)

const (
	DockerSock     = "/var/run/docker.sock"
	ContainerdSock = "/var/run/containerd/containerd.sock"
	PodmanSock     = "/var/run/podman/podman.sock"
)

type RuntimeSockets struct {
	Docker     string
	Containerd string
	Podman     string
}

func DefaultRuntimeSockets() RuntimeSockets {
	return RuntimeSockets{
		Docker:     DockerSock,
		Containerd: ContainerdSock,
		Podman:     PodmanSock,
	}
}

type ContainerRuntimeInfo struct {
	Runtime     string   `json:"runtime"`
	SocketPath  string   `json:"socket_path"`
	ContainerID string   `json:"container_id"`
	ImageRef    string   `json:"image_ref"`
	Entrypoint  []string `json:"entrypoint"`
	Cmd         []string `json:"cmd"`
	Env         []string `json:"env"`
	Source      string   `json:"source"`
}

type InspectAllResult struct {
	Info   *ContainerRuntimeInfo `json:"info,omitempty"`
	Errors []string              `json:"errors,omitempty"`
}

func InspectContainerAllRuntimes(
	ctx context.Context,
	containerID string,
	sockets RuntimeSockets,
) (*InspectAllResult, error) {
	var errs []string

	inspectors := []func(context.Context, string, RuntimeSockets) (*ContainerRuntimeInfo, error){
		inspectDocker,
		inspectContainerd,
		inspectPodman,
	}

	for _, fn := range inspectors {
		info, err := fn(ctx, containerID, sockets)
		if err == nil {
			return &InspectAllResult{
				Info:   info,
				Errors: errs,
			}, nil
		}
		errs = append(errs, err.Error())
	}

	return &InspectAllResult{
		Errors: errs,
	}, fmt.Errorf("container %q was not found in docker/containerd/podman", containerID)
}

func inspectDocker(ctx context.Context, containerID string, sockets RuntimeSockets) (*ContainerRuntimeInfo, error) {
	if sockets.Docker == "" {
		return nil, fmt.Errorf("docker: socket path is empty")
	}

	if _, err := os.Stat(sockets.Docker); err != nil {
		return nil, fmt.Errorf("docker: socket not available: %w", err)
	}

	var resp dockerLikeInspect
	if err := unixHTTPGetJSON(ctx, sockets.Docker, "/containers/"+containerID+"/json", &resp); err != nil {
		return nil, fmt.Errorf("docker: inspect failed: %w", err)
	}

	return &ContainerRuntimeInfo{
		Runtime:     "docker",
		SocketPath:  sockets.Docker,
		ContainerID: containerID,
		ImageRef:    firstNonEmpty(resp.Config.Image, resp.Image),
		Entrypoint:  cloneSlice(resp.Config.Entrypoint),
		Cmd:         cloneSlice(resp.Config.Cmd),
		Env:         cloneSlice(resp.Config.Env),
		Source:      "docker inspect",
	}, nil
}

func inspectPodman(ctx context.Context, containerID string, sockets RuntimeSockets) (*ContainerRuntimeInfo, error) {
	if sockets.Podman == "" {
		return nil, fmt.Errorf("podman: socket path is empty")
	}

	if _, err := os.Stat(sockets.Podman); err != nil {
		return nil, fmt.Errorf("podman: socket not available: %w", err)
	}

	var resp dockerLikeInspect
	if err := unixHTTPGetJSON(ctx, sockets.Podman, "/containers/"+containerID+"/json", &resp); err == nil {
		return &ContainerRuntimeInfo{
			Runtime:     "podman",
			SocketPath:  sockets.Podman,
			ContainerID: containerID,
			ImageRef:    firstNonEmpty(resp.Config.Image, resp.Image),
			Entrypoint:  cloneSlice(resp.Config.Entrypoint),
			Cmd:         cloneSlice(resp.Config.Cmd),
			Env:         cloneSlice(resp.Config.Env),
			Source:      "podman docker-compatible inspect",
		}, nil
	}

	var libpodResp podmanLibpodInspect
	if err := unixHTTPGetJSON(ctx, sockets.Podman, "/v4.0.0/libpod/containers/"+containerID+"/json", &libpodResp); err != nil {
		return nil, fmt.Errorf("podman: inspect failed: %w", err)
	}

	return &ContainerRuntimeInfo{
		Runtime:     "podman",
		SocketPath:  sockets.Podman,
		ContainerID: containerID,
		ImageRef:    firstNonEmpty(libpodResp.Config.Image, libpodResp.ImageName, libpodResp.Image),
		Entrypoint:  cloneSlice(libpodResp.Config.Entrypoint),
		Cmd:         cloneSlice(libpodResp.Config.Cmd),
		Env:         cloneSlice(libpodResp.Config.Env),
		Source:      "podman libpod inspect",
	}, nil
}

func inspectContainerd(ctx context.Context, containerID string, sockets RuntimeSockets) (*ContainerRuntimeInfo, error) {
	if sockets.Containerd == "" {
		return nil, fmt.Errorf("containerd: socket path is empty")
	}

	if _, err := os.Stat(sockets.Containerd); err != nil {
		return nil, fmt.Errorf("containerd: socket not available: %w", err)
	}

	client, err := containerd.New(sockets.Containerd)
	if err != nil {
		return nil, fmt.Errorf("containerd: create client: %w", err)
	}
	defer client.Close()

	nsList, err := client.NamespaceService().List(ctx)
	if err != nil {
		return nil, fmt.Errorf("containerd: list namespaces: %w", err)
	}
	if len(nsList) == 0 {
		return nil, fmt.Errorf("containerd: server returned no namespaces")
	}

	var perNSErrs []string

	for _, ns := range uniqueStrings(nsList) {
		nsctx := namespaces.WithNamespace(ctx, ns)

		ctr, err := client.LoadContainer(nsctx, containerID)
		if err != nil {
			perNSErrs = append(perNSErrs, fmt.Sprintf("ns=%s: %v", ns, err))
			continue
		}

		info, err := ctr.Info(nsctx)
		if err != nil {
			perNSErrs = append(perNSErrs, fmt.Sprintf("ns=%s info: %v", ns, err))
			continue
		}

		spec, err := ctr.Spec(nsctx)
		if err != nil {
			perNSErrs = append(perNSErrs, fmt.Sprintf("ns=%s spec: %v", ns, err))
			continue
		}

		var args []string
		var envs []string
		if spec != nil && spec.Process != nil {
			args = cloneSlice(spec.Process.Args)
			envs = cloneSlice(spec.Process.Env)
		}

		entrypoint, cmd := splitProcessArgs(args)

		return &ContainerRuntimeInfo{
			Runtime:     "containerd",
			SocketPath:  sockets.Containerd,
			ContainerID: containerID,
			ImageRef:    info.Image,
			Entrypoint:  entrypoint,
			Cmd:         cmd,
			Env:         envs,
			Source:      "containerd container info + OCI spec.process",
		}, nil
	}

	return nil, fmt.Errorf("containerd: not found; details: %s", strings.Join(perNSErrs, "; "))
}

type dockerLikeInspect struct {
	Image  string `json:"Image"`
	Config struct {
		Image      string   `json:"Image"`
		Entrypoint []string `json:"Entrypoint"`
		Cmd        []string `json:"Cmd"`
		Env        []string `json:"Env"`
	} `json:"Config"`
}

type podmanLibpodInspect struct {
	Image     string `json:"Image"`
	ImageName string `json:"ImageName"`
	Config    struct {
		Image      string   `json:"Image"`
		Entrypoint []string `json:"Entrypoint"`
		Cmd        []string `json:"Cmd"`
		Env        []string `json:"Env"`
	} `json:"Config"`
}

func unixHTTPGetJSON(ctx context.Context, socketPath, path string, out any) error {
	tr := &http.Transport{
		DialContext: func(ctx context.Context, network, addr string) (net.Conn, error) {
			var d net.Dialer
			return d.DialContext(ctx, "unix", socketPath)
		},
	}
	defer tr.CloseIdleConnections()

	client := &http.Client{
		Transport: tr,
		Timeout:   10 * time.Second,
	}

	req, err := http.NewRequestWithContext(ctx, http.MethodGet, "http://unix"+path, nil)
	if err != nil {
		return err
	}

	resp, err := client.Do(req)
	if err != nil {
		return err
	}
	defer resp.Body.Close()

	if resp.StatusCode == http.StatusNotFound {
		return fmt.Errorf("not found")
	}
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return fmt.Errorf("unexpected http status: %s", resp.Status)
	}

	return json.NewDecoder(resp.Body).Decode(out)
}

func splitProcessArgs(args []string) (entrypoint, cmd []string) {
	if len(args) == 0 {
		return nil, nil
	}
	return []string{args[0]}, cloneSlice(args[1:])
}

func firstNonEmpty(values ...string) string {
	for _, v := range values {
		if strings.TrimSpace(v) != "" {
			return v
		}
	}
	return ""
}

func cloneSlice(in []string) []string {
	if len(in) == 0 {
		return nil
	}
	out := make([]string, len(in))
	copy(out, in)
	return out
}

func uniqueStrings(in []string) []string {
	seen := make(map[string]struct{}, len(in))
	out := make([]string, 0, len(in))
	for _, s := range in {
		if s == "" {
			continue
		}
		if _, ok := seen[s]; ok {
			continue
		}
		seen[s] = struct{}{}
		out = append(out, s)
	}
	return out
}
