import os
import sys
import logging
from typing import Dict, Set, Tuple

CONFINE_ROOT = os.path.abspath(".")
sys.path.insert(0, os.path.join(CONFINE_ROOT, "python-utils"))

import graph
import syscall
import binaryAnalysis

def build_logger(debug: bool = False) -> logging.Logger:
    logger = logging.getLogger("confine-syscall-analysis")
    logger.handlers.clear()
    logger.setLevel(logging.DEBUG if debug else logging.INFO)

    h = logging.StreamHandler()
    h.setLevel(logging.DEBUG if debug else logging.INFO)
    fmt = logging.Formatter("%(levelname)s: %(message)s")
    h.setFormatter(fmt)
    logger.addHandler(h)
    return logger


def load_libc_cfg(callgraph_path: str, separator: str, logger: logging.Logger) -> "graph.Graph":
    libc_cfg = graph.Graph(logger)
    libc_cfg.createGraphFromInput(callgraph_path, separator)
    return libc_cfg


def analyze_one_binary(
    binary_path: str,
    libc_cfg: "graph.Graph",
    logger: logging.Logger,
) -> Tuple[Set[int], Set[int], Tuple[int, int]]:
    ba = binaryAnalysis.BinaryAnalysis(binary_path, logger)
    result = ba.extractDirectSyscalls()
    if result is None:
        print("Failed to analyze binary!")
        sys.exit(1)
    direct_set, success_count, fail_count = result
    indirect_set = ba.extractIndirectSyscalls(libc_cfg)

    return direct_set, indirect_set, (success_count, fail_count)


def analyze_binaries(binaries, callgraph_path, separator=":", maptype="awk", debug=False):
    logger = build_logger(debug=debug)

    libc_cfg = load_libc_cfg(callgraph_path, separator, logger)

    
    sc = syscall.Syscall(logger)
    sc.createMap(maptype)
    inv = sc.getInverseMap()  
    
    num_to_name = {v: k for k, v in inv.items() if isinstance(v, int)}

    out: Dict[str, dict] = {}

    for b in binaries:
        direct_set, indirect_set, (ok, fail) = analyze_one_binary(b, libc_cfg, logger)
        all_set = set(direct_set) | set(indirect_set)

        out[b] = {
            "direct_nums": sorted(direct_set),
            "indirect_nums": sorted(indirect_set),
            "all_nums": sorted(all_set),
            "direct_ok": ok,
            "direct_fail": fail,
            
            "all_names": [num_to_name.get(n, f"sys_{n}") for n in sorted(all_set)],
        }

    return out


if __name__ == "__main__":
    import argparse
    p = argparse.ArgumentParser()
    p.add_argument("binaries", nargs="+")
    p.add_argument("--callgraph", required=True)
    p.add_argument("--sep", default=":")
    p.add_argument("--maptype", default="awk")
    p.add_argument("--debug", action="store_true")
    args = p.parse_args()

    result = analyze_binaries(
        binaries=args.binaries,
        callgraph_path=args.callgraph,
        separator=args.sep,
        maptype=args.maptype,
        debug=args.debug,
    )

    for bin_path, r in result.items():
        out = os.path.join(CONFINE_ROOT, os.path.basename(bin_path) + ".syscall")
        with open(out, "w") as file:
            file.write("\n".join(r["all_names"]))
