#!/usr/bin/env python3
"""
Thin subprocess entry-point for the Go constant-in interface.

Outputs syscall numbers (one integer per line) to stdout.
All diagnostic messages go to stderr so they don't pollute the Go parser.

Analysis:
  - Direct syscalls: objdump -d on the binary AND all reachable .so files
    (resolved from --ld-paths / --sysroot)
  - Indirect syscalls via libc callgraph: objdump -T on the binary (and each
    reachable .so) to find imported function names, then traced through the
    appropriate callgraph (glibc or musl — auto-detected).
  - Fine-grain mode (default, disable with --no-fine-grain): for each reachable
    .so that has a dedicated callgraph in --other-cfg-folder, the analysis
    chain is: imported_function → lib-callgraph → libc-function → syscall(N).
    This is more precise than objdump on the .so itself.

Usage:
  confine_run.py [--callgraph=<path>] [--musl-callgraph=<path>]
                 [--other-cfg-folder=<path>] [--no-fine-grain]
                 [--sep=<sep>] [--ld-paths=<colon-separated-dirs>]
                 [--sysroot=<path>]
                 <binary>
"""
import argparse
import logging
import os
import re
import subprocess
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(SCRIPT_DIR, "python-utils"))

import binaryAnalysis
import graph
import util


# ── Constants ────────────────────────────────────────────────────────────────

# libc soname prefixes we skip when searching for extra syscalls — their
# syscalls are already covered by the indirect callgraph analysis.
LIBC_SKIP_PREFIXES = (
    "libc.so", "libc.musl", "libdl.so", "libpthread.so", "libm.so",
    "libresolv.so", "librt.so", "libutil.so", "libnss_",
    "libcrypt.so", "ld-linux", "ld-musl", "linux-vdso",
)

# musl soname prefixes used for libc-type auto-detection.
MUSL_PREFIXES = ("musl", "ld-musl", "libc.musl")

# Libraries whose syscalls are fully covered by the libc callgraph — skip
# fine-grain analysis for them to avoid double-counting.
LIBS_IN_LIBC = frozenset({
    "libcrypt.callgraph.out", "libdl.callgraph.out",
    "libnsl.callgraph.out", "libnss_compat.callgraph.out",
    "libnss_files.callgraph.out", "libnss_nis.callgraph.out",
    "libpthread.callgraph.out", "libm.callgraph.out",
    "libresolv.callgraph.out", "librt.callgraph.out",
    "libutil.callgraph.out", "libnss_dns.callgraph.out",
})


# ── Helpers ──────────────────────────────────────────────────────────────────

def make_syscall_node_list():
    """Build the terminal-node list used by getLeavesFromStartNode."""
    nodes = []
    for i in range(400):
        nodes.append(f"syscall({i})")
        nodes.append(f"syscall ( {i} )")
        nodes.append(f"syscall( {i} )")
    return nodes


def parse_syscall_leaves(leaf_set):
    """Convert a set of 'syscall(N)' strings to a set of ints."""
    result = set()
    for s in leaf_set:
        s = (s.replace("syscall( ", "syscall(")
              .replace("syscall ( ", "syscall(")
              .replace(" )", ")"))
        try:
            result.add(int(s[8:-1]))
        except (ValueError, IndexError):
            pass
    return result


def so_to_cfg_name(so_path):
    """
    Derive the expected callgraph filename from a .so path.
    libssl-1.1.so.1.1 → libssl.callgraph.out
    Returns None if the filename doesn't look like a library.
    """
    name = os.path.basename(so_path)
    name = re.sub(r"-.*so", ".so", name)   # libssl-1.1.so.1.1 → libssl.so.1.1
    idx = name.find(".so")
    if idx == -1:
        return None
    return name[:idx] + ".callgraph.out"


def get_needed_sonames(elf_path):
    """Return the set of NEEDED sonames declared in an ELF file."""
    try:
        p1 = subprocess.Popen(
            ["objdump", "-p", elf_path],
            stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
        )
        p2 = subprocess.Popen(
            ["grep", "NEEDED"], stdin=p1.stdout, stdout=subprocess.PIPE,
        )
        p1.stdout.close()
        out, _ = p2.communicate()
        names = set()
        for line in out.decode("utf-8", errors="replace").splitlines():
            name = line.replace("NEEDED", "").strip()
            if name and "linux-vdso" not in name and "ld-linux" not in name:
                names.add(name)
        return names
    except Exception:
        return set()


def detect_libc(binary_path, logger):
    """
    Return "musl" or "glibc" based on the NEEDED sonames of the binary.
    Returns "unknown" when the binary is statically linked (empty NEEDED).
    """
    sonames = get_needed_sonames(binary_path)
    for soname in sonames:
        if any(soname.startswith(p) for p in MUSL_PREFIXES):
            logger.debug("detected musl libc via soname %s", soname)
            return "musl"
    if sonames:
        logger.debug("detected glibc among %s", sonames)
        return "glibc"
    logger.debug("no NEEDED sonames — statically linked, will try both graphs")
    return "unknown"


def find_so_in_paths(soname, search_dirs):
    """
    Try to locate a .so file by soname in search_dirs.
    Also tries the soname without version suffix (libfoo.so.1 → libfoo.so).
    """
    candidates = [soname]
    base = soname
    while True:
        new = base.rsplit(".", 1)[0]
        if new == base or not new.endswith(".so"):
            break
        base = new
    if base != soname:
        candidates.append(base)

    for d in search_dirs:
        for name in candidates:
            path = os.path.join(d, name)
            if os.path.isfile(path):
                return path
    return None


def collect_so_paths(root_elf, search_dirs, logger):
    """
    BFS over NEEDED sonames starting from root_elf.
    Returns a set of resolved .so file paths (excluding libc-family libs).
    """
    resolved = set()
    queue = [root_elf]
    seen_sonames = set()

    while queue:
        elf = queue.pop()
        for soname in get_needed_sonames(elf):
            if soname in seen_sonames:
                continue
            seen_sonames.add(soname)
            if any(soname.startswith(p) for p in LIBC_SKIP_PREFIXES):
                continue
            path = find_so_in_paths(soname, search_dirs)
            if path and path not in resolved:
                resolved.add(path)
                queue.append(path)
            else:
                logger.debug("could not resolve soname %s in ld-paths", soname)

    return resolved


# ── Core analysis ────────────────────────────────────────────────────────────

def analyse_binary_standard(binary_path, libc_cfg, logger):
    """
    Standard analysis: direct syscalls (objdump) + indirect syscalls (libc
    callgraph traversal via imported functions).
    Returns a set of ints.
    """
    ba = binaryAnalysis.BinaryAnalysis(binary_path, logger)
    result = ba.extractDirectSyscalls()
    direct = result[0] if result and result[0] else set()
    indirect = ba.extractIndirectSyscalls(libc_cfg)
    return set(direct) | set(indirect)


def fine_grain_analyse_so(so_path, imported_funcs, libc_cfg,
                          syscall_nodes, cfg_folder, libs_with_cfg, logger):
    """
    Fine-grained analysis of a single .so dependency.

    For .so files that have a dedicated callgraph in cfg_folder:
      imported_function → lib-callgraph → libc-function → libc-callgraph → syscall(N)

    For .so files without a dedicated callgraph (or in LIBS_IN_LIBC):
      falls back to standard objdump + libc-callgraph analysis.
    """
    cfg_name = so_to_cfg_name(so_path)
    if cfg_name is None:
        return analyse_binary_standard(so_path, libc_cfg, logger)

    # libc-family libs: already covered by the main binary's indirect analysis.
    if cfg_name in LIBS_IN_LIBC:
        return set()

    if cfg_name not in libs_with_cfg:
        logger.debug("no dedicated CFG for %s — standard analysis", cfg_name)
        return analyse_binary_standard(so_path, libc_cfg, logger)

    logger.debug("fine-grain: %s  →  %s", os.path.basename(so_path), cfg_name)

    # Load the library's internal callgraph.
    lib_graph = graph.Graph(logger)
    lib_graph.createGraphFromInput(os.path.join(cfg_folder, cfg_name), "->")

    # For each function imported by the main binary, find which libc-level
    # functions this library reaches internally.
    libc_funcs = set()
    for func in imported_funcs:
        leaves = lib_graph.getLeavesFromStartNode(func, [], [])
        if leaves and func not in leaves:
            libc_funcs.update(leaves)

    # Trace those libc functions through the glibc/musl callgraph → syscalls.
    tmp = set()
    for func in libc_funcs:
        tmp.update(libc_cfg.getLeavesFromStartNode(func, syscall_nodes, []))
    syscalls = parse_syscall_leaves(tmp)

    # Always add direct syscalls from the .so itself (inline asm, etc.).
    ba = binaryAnalysis.BinaryAnalysis(so_path, logger)
    direct = ba.extractDirectSyscalls()
    if direct and direct[0]:
        syscalls |= direct[0]

    return syscalls


def run_with_graph(binary_path, libc_cfg, search_dirs, logger,
                   fine_grain=True, cfg_folder=None):
    """
    Full analysis: main binary + all reachable .so files using libc_cfg.
    Returns (syscall_set, ok) where ok=False means the main binary failed.
    """
    ba = binaryAnalysis.BinaryAnalysis(binary_path, logger)
    result = ba.extractDirectSyscalls()
    if result is None or result[0] is None:
        return set(), False

    all_set = set(result[0]) | set(ba.extractIndirectSyscalls(libc_cfg))

    if search_dirs:
        so_paths = collect_so_paths(binary_path, search_dirs, logger)

        if fine_grain and cfg_folder and os.path.isdir(cfg_folder):
            syscall_nodes = make_syscall_node_list()
            libs_with_cfg = set(os.listdir(cfg_folder))
            imported_funcs = util.extractImportedFunctions(binary_path, logger) or []
            for so_path in so_paths:
                all_set |= fine_grain_analyse_so(
                    so_path, imported_funcs, libc_cfg,
                    syscall_nodes, cfg_folder, libs_with_cfg, logger,
                )
        else:
            for so_path in so_paths:
                logger.debug("analysing library: %s", so_path)
                all_set |= analyse_binary_standard(so_path, libc_cfg, logger)

    return all_set, True


# ── Entry point ──────────────────────────────────────────────────────────────

def main():
    p = argparse.ArgumentParser()
    p.add_argument("binary")
    p.add_argument(
        "--callgraph",
        default=os.path.join(SCRIPT_DIR, "libc-callgraphs", "glibc.callgraph"),
        help="Path to the glibc callgraph file",
    )
    p.add_argument(
        "--musl-callgraph",
        default=os.path.join(SCRIPT_DIR, "libc-callgraphs", "musllibc.callgraph"),
        help="Path to the musl callgraph file",
    )
    p.add_argument(
        "--other-cfg-folder",
        default=os.path.join(SCRIPT_DIR, "other-callgraphs"),
        help="Directory containing per-library callgraph files (fine-grain mode)",
    )
    p.add_argument(
        "--no-fine-grain",
        action="store_true",
        default=False,
        help="Disable fine-grain library analysis (use standard objdump for all .so files)",
    )
    p.add_argument("--sep", default=":")
    p.add_argument(
        "--ld-paths",
        default="",
        help="Colon-separated directories to search for shared libraries",
    )
    p.add_argument(
        "--sysroot",
        default="",
        help="Filesystem root for resolving libraries (prepended to standard lib dirs)",
    )
    args = p.parse_args()

    logger = logging.getLogger("confine_run")
    logger.handlers.clear()
    h = logging.StreamHandler(sys.stderr)
    h.setFormatter(logging.Formatter("%(levelname)s: %(message)s"))
    logger.addHandler(h)
    logger.setLevel(logging.WARNING)

    fine_grain = not args.no_fine_grain

    # Build the list of directories to search for .so files.
    search_dirs = []
    if args.ld_paths:
        search_dirs.extend(d for d in args.ld_paths.split(":") if d)
    if args.sysroot:
        for std in ("lib", "lib64", "usr/lib", "usr/lib64",
                    "lib/x86_64-linux-gnu", "usr/lib/x86_64-linux-gnu"):
            search_dirs.append(os.path.join(args.sysroot, std))

    # ── Detect libc type ─────────────────────────────────────────────────────
    libc_type = detect_libc(args.binary, logger)

    # ── Load the appropriate callgraph and run analysis ───────────────────────
    def load_graph(path, sep):
        g = graph.Graph(logger)
        g.createGraphFromInput(path, sep)
        return g

    def run(libc_cfg):
        return run_with_graph(
            args.binary, libc_cfg, search_dirs, logger,
            fine_grain=fine_grain,
            cfg_folder=args.other_cfg_folder,
        )

    if libc_type == "musl":
        libc_cfg = load_graph(args.musl_callgraph, "->")
        all_set, ok = run(libc_cfg)

    elif libc_type == "glibc":
        libc_cfg = load_graph(args.callgraph, args.sep)
        all_set, ok = run(libc_cfg)

    else:
        # Statically linked — try glibc first, fall back to musl if empty.
        libc_cfg = load_graph(args.callgraph, args.sep)
        all_set, ok = run(libc_cfg)
        if ok and len(all_set) == 0:
            musl_cfg = load_graph(args.musl_callgraph, "->")
            musl_set, musl_ok = run_with_graph(
                args.binary, musl_cfg, search_dirs, logger,
                fine_grain=fine_grain,
                cfg_folder=args.other_cfg_folder,
            )
            if musl_ok and len(musl_set) > 0:
                all_set = musl_set

    if not ok:
        print("failed to extract direct syscalls from: " + args.binary, file=sys.stderr)
        sys.exit(1)

    for n in sorted(n for n in all_set if 0 <= n <= 467):
        print(n)


if __name__ == "__main__":
    main()
