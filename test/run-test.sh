#!/usr/bin/env bash
set -euo pipefail

TEST_ROOT="${TEST_ROOT:-/opt/test/build}"
OUT_ROOT="${OUT_ROOT:-/workspace/syscall_results}"
CONFINE_DIR="${CONFINE_DIR:-/opt/confine}"
CHESTNUT_DIR="${CHESTNUT_DIR:-/opt/chestnut/Binalyzer}"
CONFINE_CALLGRAPH="${CONFINE_CALLGRAPH:-/opt/confine/libc-callgraphs/glibc.callgraph}"

RUN_CONFINE="${RUN_CONFINE:-0}"
RUN_SYSFILTER="${RUN_SYSFILTER:-0}"
RUN_SYSPART="${RUN_SYSPART:-0}"
RUN_SYSPART_NEW="${RUN_SYSPART_NEW:-0}"
RUN_CHESTNUT="${RUN_CHESTNUT:-0}"
RUN_GO2SECCOMP="${RUN_GO2SECCOMP:-0}"

GO2SECCOMP_BIN="${GO2SECCOMP_BIN:-/root/go/bin/go2seccomp}"

TIMEOUT_BIN="${TIMEOUT_BIN:-}"
TIMEOUT_PY="${TIMEOUT_PY:-}"

TIMEOUT_SYSFILTER="${TIMEOUT_SYSFILTER:-}"
TIMEOUT_SYSPART="${TIMEOUT_SYSPART:-}"
TIMEOUT_SYSPART_NEW="${TIMEOUT_SYSPART_NEW:-}"
TIMEOUT_CONFINE="${TIMEOUT_CONFINE:-}"
TIMEOUT_CHESTNUT="${TIMEOUT_CHESTNUT:-}"
TIMEOUT_GO2SECCOMP="${TIMEOUT_GO2SECCOMP:-}"

TEST_PREFIXES="${TEST_PREFIXES:-}"

die() { echo "ERROR: $*" >&2; exit 1; }
have() { command -v "$1" >/dev/null 2>&1; }

run_cmd() {
  local out="$1" err="$2" code="$3"
  shift 3
  if [[ "${1:-}" != "--" ]]; then die "run_cmd expects --"; fi
  shift

  set +e
  "$@" >"$out" 2>"$err"
  local rc=$?
  set -e
  echo "$rc" >"$code"
  return 0
}

is_elf_exec() {
  local f="$1"
  [[ -f "$f" && -x "$f" ]] || return 1
  file "$f" 2>/dev/null | grep -qE 'ELF (64-bit|32-bit) LSB' || return 1
  return 0
}

safe_id_from_path() {
  local p="$1"
  p="${p#/}"
  p="${p//\//__}"
  echo "$p"
}

get_entry_hex() {
  local bin="$1"
  readelf -h "$bin" 2>/dev/null | awk '/Entry point address/ {print $4}' | sed 's/^0x//'
}

get_entry_symbol_or_fuzzy() {
  local bin="$1"
  local entry func

  entry="$(get_entry_hex "$bin")"
  [[ -n "$entry" ]] || { echo ""; return 0; }

  func="$(readelf -s "$bin" 2>/dev/null \
    | grep "$entry" \
    | awk '{ print $8 }' \
    | egrep -v '^\s*$' \
    | head -n1 || true)"

  if [[ -z "${func:-}" ]]; then
    func="fuzzyfunc-0x${entry}@${entry}"
  fi

  echo "$func"
}

path_to_chestnut_cache_key() {
  local p="$1"
  p="${p#/}"
  p="${p//\//_}"
  echo "$p"
}

normalize_prefixes() {
  local -a raw=("$@")
  local -a norm=()
  local p
  for p in "${raw[@]}"; do
    [[ -n "$p" ]] || continue
    if [[ "$p" != /* ]]; then
      p="$TEST_ROOT/$p"
    fi
    while [[ "$p" != "/" && "$p" == */ ]]; do p="${p%/}"; done
    norm+=("$p")
  done
  printf "%s\n" "${norm[@]}"
}

matches_prefixes() {
  local path="$1"; shift
  local p
  if (( ${#@} == 0 )); then
    return 0
  fi
  for p in "$@"; do
    [[ "$path" == "$p"* ]] && return 0
  done
  return 1
}

is_posint_or_empty() {
  local v="$1"
  [[ -z "$v" ]] && return 0
  [[ "$v" =~ ^[0-9]+$ ]] || return 1
  return 0
}

mk_timeout_prefix() {
  local sec="$1"
  if [[ -n "$sec" && "$sec" != "0" ]]; then
    echo timeout "${sec}s"
  fi
}

mkdir -p "$OUT_ROOT"
mkdir -p "$OUT_ROOT/_meta"

PREFIXES=()
if (( $# > 0 )); then
  mapfile -t PREFIXES < <(normalize_prefixes "$@")
elif [[ -n "$TEST_PREFIXES" ]]; then
  _tmp=($TEST_PREFIXES)
  mapfile -t PREFIXES < <(normalize_prefixes "${_tmp[@]}")
fi

echo "[*] Scanning for ELF executables under: $TEST_ROOT"
mapfile -t ALL_FILES < <(find "$TEST_ROOT" -type f -perm /111 2>/dev/null | sort)

BINARIES=()
for f in "${ALL_FILES[@]}"; do
  if ! matches_prefixes "$f" "${PREFIXES[@]}"; then
    continue
  fi

  case "$f" in
    *.syscall|*.syscalls|*.strace|*.log|*.build.log|*.json) continue ;;
  esac
  if is_elf_exec "$f"; then
    BINARIES+=("$f")
  fi
done

echo "[*] Found ELF executables: ${#BINARIES[@]}"
printf "%s\n" "${BINARIES[@]}" > "$OUT_ROOT/_meta/binaries.list"

if (( RUN_SYSFILTER == 1 )); then
  have sysfilter_extract || die "sysfilter_extract not found in PATH"
fi
if (( RUN_SYSPART == 1 )); then
  have syspart || die "syspart not found in PATH"
fi
if (( RUN_SYSPART_NEW == 1 )); then
  have syspart_new || die "syspart not found in PATH"
fi
if (( RUN_CONFINE == 1 )); then
  [[ -d "$CONFINE_DIR" ]] || die "CONFINE_DIR not found: $CONFINE_DIR"
  [[ -f "$CONFINE_CALLGRAPH" ]] || die "CONFINE_CALLGRAPH not found: $CONFINE_CALLGRAPH"
  have python3 || die "python3 not found"
fi
if (( RUN_CHESTNUT == 1 )); then
  [[ -d "$CHESTNUT_DIR" ]] || die "CHESTNUT_DIR not found: $CHESTNUT_DIR"
  [[ -f "$CHESTNUT_DIR/syscalls.py" ]] || die "CHESTNUT syscalls.py not found: $CHESTNUT_DIR/syscalls.py"
  have python3 || die "python3 not found"
fi
if (( RUN_GO2SECCOMP == 1 )); then
  [[ -x "$GO2SECCOMP_BIN" ]] || die "GO2SECCOMP_BIN not found/executable: $GO2SECCOMP_BIN"
  have jq || die "jq not found in PATH"
fi

have file || die "file not found in PATH"

is_posint_or_empty "$TIMEOUT_SYSFILTER" || die "TIMEOUT_SYSFILTER must be a non-negative integer (seconds)"
is_posint_or_empty "$TIMEOUT_SYSPART" || die "TIMEOUT_SYSPART must be a non-negative integer (seconds)"
is_posint_or_empty "$TIMEOUT_SYSPART_NEW" || die "TIMEOUT_SYSPART_NEW must be a non-negative integer (seconds)"
is_posint_or_empty "$TIMEOUT_CONFINE" || die "TIMEOUT_CONFINE must be a non-negative integer (seconds)"
is_posint_or_empty "$TIMEOUT_CHESTNUT" || die "TIMEOUT_CHESTNUT must be a non-negative integer (seconds)"
is_posint_or_empty "$TIMEOUT_GO2SECCOMP" || die "TIMEOUT_GO2SECCOMP must be a non-negative integer (seconds)"

need_timeout=0
if [[ -n "$TIMEOUT_BIN" || -n "$TIMEOUT_PY" ]]; then need_timeout=1; fi
if [[ -n "$TIMEOUT_SYSFILTER" && "$TIMEOUT_SYSFILTER" != "0" ]]; then need_timeout=1; fi
if [[ -n "$TIMEOUT_SYSPART" && "$TIMEOUT_SYSPART" != "0" ]]; then need_timeout=1; fi
if [[ -n "$TIMEOUT_SYSPART_NEW" && "$TIMEOUT_SYSPART_NEW" != "0" ]]; then need_timeout=1; fi
if [[ -n "$TIMEOUT_CONFINE" && "$TIMEOUT_CONFINE" != "0" ]]; then need_timeout=1; fi
if [[ -n "$TIMEOUT_CHESTNUT" && "$TIMEOUT_CHESTNUT" != "0" ]]; then need_timeout=1; fi
if [[ -n "$TIMEOUT_GO2SECCOMP" && "$TIMEOUT_GO2SECCOMP" != "0" ]]; then need_timeout=1; fi

if (( need_timeout == 1 )); then
  have timeout || die "timeout is required but not found in PATH"
fi

SYSFILTER_TIMEOUT_PREFIX="$(mk_timeout_prefix "$TIMEOUT_SYSFILTER")"
SYSPART_TIMEOUT_PREFIX="$(mk_timeout_prefix "$TIMEOUT_SYSPART")"
SYSPART_NEW_TIMEOUT_PREFIX="$(mk_timeout_prefix "$TIMEOUT_SYSPART_NEW")"
CONFINE_TIMEOUT_PREFIX="$(mk_timeout_prefix "$TIMEOUT_CONFINE")"
CHESTNUT_TIMEOUT_PREFIX="$(mk_timeout_prefix "$TIMEOUT_CHESTNUT")"
GO2SECCOMP_TIMEOUT_PREFIX="$(mk_timeout_prefix "$TIMEOUT_GO2SECCOMP")"

if [[ -z "$SYSFILTER_TIMEOUT_PREFIX" ]]; then SYSFILTER_TIMEOUT_PREFIX="${TIMEOUT_BIN:-}"; fi
if [[ -z "$SYSPART_TIMEOUT_PREFIX" ]]; then SYSPART_TIMEOUT_PREFIX="${TIMEOUT_BIN:-}"; fi
if [[ -z "$SYSPART_NEW_TIMEOUT_PREFIX" ]]; then SYSPART_NEW_TIMEOUT_PREFIX="${TIMEOUT_BIN:-}"; fi
if [[ -z "$CONFINE_TIMEOUT_PREFIX" ]]; then CONFINE_TIMEOUT_PREFIX="${TIMEOUT_PY:-}"; fi
if [[ -z "$CHESTNUT_TIMEOUT_PREFIX" ]]; then CHESTNUT_TIMEOUT_PREFIX="${TIMEOUT_PY:-}"; fi
if [[ -z "$GO2SECCOMP_TIMEOUT_PREFIX" ]]; then GO2SECCOMP_TIMEOUT_PREFIX="${TIMEOUT_BIN:-}"; fi

echo "[*] Output dir: $OUT_ROOT"
if (( ${#PREFIXES[@]} > 0 )); then
  echo "[*] Prefix filter enabled:"
  printf "    - %s\n" "${PREFIXES[@]}"
fi
echo "[*] Starting runs..."

for bin in "${BINARIES[@]}"; do
  rel_id="$(safe_id_from_path "$bin")"
  base="$(basename "$bin")"
  out_dir="$OUT_ROOT/$rel_id"
  mkdir -p "$out_dir"

  {
    echo "binary=$bin"
    echo "basename=$base"
    echo "date_utc=$(date -u +%FT%TZ)"
  } > "$out_dir/meta.txt"

  echo
  echo "=== [$base] ==="
  echo "bin: $bin"
  echo "out: $out_dir"

  if (( RUN_SYSFILTER == 1 )); then
    echo "  - sysfilter_extract"
    run_cmd \
      "$out_dir/sysfilter_extract.stdout" \
      "$out_dir/sysfilter_extract.stderr" \
      "$out_dir/sysfilter_extract.exitcode" \
      -- ${SYSFILTER_TIMEOUT_PREFIX:-} sysfilter_extract -o "$out_dir/sysfilter_extract.syscalls.json" "$bin"
  fi

  if (( RUN_SYSPART == 1 )); then
    echo "  - syspart"
    entry_hex="$(get_entry_hex "$bin")"
    func="$(get_entry_symbol_or_fuzzy "$bin")"

    {
      echo "entry_hex=${entry_hex:-<none>}"
      echo "entry_symbol=$func"
    } > "$out_dir/syspart.meta"

    run_cmd \
      "$out_dir/syspart.syscalls.txt" \
      "$out_dir/syspart.stderr" \
      "$out_dir/syspart.exitcode" \
      -- ${SYSPART_TIMEOUT_PREFIX:-} syspart -a 7,"$func" -s "$func" -p "$bin"

    cp -f "$out_dir/syspart.syscalls.txt" "$out_dir/syspart.stdout"
  fi

  if (( RUN_SYSPART_NEW == 1 )); then
    echo "  - syspart_new"
    entry_hex="$(get_entry_hex "$bin")"
    func="$(get_entry_symbol_or_fuzzy "$bin")"
    entry_addr=""
    if [[ -n "${entry_hex:-}" ]]; then
      entry_addr="0x${entry_hex}"
    fi

    {
      echo "entry_hex=${entry_hex:-<none>}"
      echo "entry_addr=${entry_addr:-<none>}"
      echo "entry_symbol=$func"
      echo "cmd=syspart_new -a 7,* -s ${entry_addr:-<none>} -p $bin"
    } > "$out_dir/syspart_new.meta"

    if [[ -z "${entry_addr:-}" ]]; then
      echo "ERROR: cannot determine entry point address for $bin" >> "$out_dir/syspart_new.stderr"
      echo "2" > "$out_dir/syspart_new.exitcode"
    else
      run_cmd \
        "$out_dir/syspart_new.result.json" \
        "$out_dir/syspart_new.stderr" \
        "$out_dir/syspart_new.exitcode" \
        -- ${SYSPART_NEW_TIMEOUT_PREFIX:-} syspart_new -a 7,* -s "$entry_addr" -p "$bin"

      cp -f "$out_dir/syspart_new.result.json" "$out_dir/syspart_new.stdout"
    fi
  fi

  if (( RUN_CONFINE == 1 )); then
    echo "  - confine"
    work="$out_dir/_work_confine"
    mkdir -p "$work"

    pushd "$CONFINE_DIR" >/dev/null

    run_cmd \
        "$out_dir/confine.stdout" \
        "$out_dir/confine.stderr" \
        "$out_dir/confine.exitcode" \
        -- ${CONFINE_TIMEOUT_PREFIX:-} python3 "$CONFINE_DIR/main.py" --callgraph "$CONFINE_CALLGRAPH" "$bin"

    expected="$CONFINE_DIR/${base}.syscall"
    if [[ -f "$expected" ]]; then
        cp -f "$expected" "$out_dir/confine.syscalls.txt"
        cp -f "$expected" "$work/${base}.syscall" || true
        rm -f "$expected" || true
    else
        echo "ERROR: confine artifact not found: $expected" >> "$out_dir/confine.stderr"
    fi

    popd >/dev/null
  fi

  if (( RUN_CHESTNUT == 1 )); then
    echo "  - chestnut (syscalls.py)"
    work="$out_dir/_work_chestnut"
    mkdir -p "$work"

    pushd "$CHESTNUT_DIR" >/dev/null

    rm -rf "$CHESTNUT_DIR/cached_results"
    mkdir -p "$CHESTNUT_DIR/cached_results"

    run_cmd \
        "$out_dir/chestnut.stdout" \
        "$out_dir/chestnut.stderr" \
        "$out_dir/chestnut.exitcode" \
        -- ${CHESTNUT_TIMEOUT_PREFIX:-} python3 "$CHESTNUT_DIR/syscalls.py" "$bin"

    cache_key="$(path_to_chestnut_cache_key "$bin")"
    expected="$CHESTNUT_DIR/cached_results/syscalls__${cache_key}.json"

    if [[ -f "$expected" ]]; then
        cp -f "$expected" "$out_dir/chestnut.syscalls.json"
        mkdir -p "$work/cached_results"
        cp -f "$expected" "$work/cached_results/$(basename "$expected")" || true
        rm -f "$expected" || true
    else
        echo "ERROR: chestnut artifact not found: $expected" >> "$out_dir/chestnut.stderr"
    fi

    popd >/dev/null
  fi

  if (( RUN_GO2SECCOMP == 1 )); then
    echo "  - go2seccomp + jq"
    json_out="$out_dir/go2seccomp.json"
    syscalls_out="$out_dir/go2seccomp.syscalls.txt"

    run_cmd \
      "$out_dir/go2seccomp.stdout" \
      "$out_dir/go2seccomp.stderr" \
      "$out_dir/go2seccomp.exitcode" \
      -- ${GO2SECCOMP_TIMEOUT_PREFIX:-} "$GO2SECCOMP_BIN" "$bin" "$json_out"

    if [[ -f "$json_out" ]]; then
      set +e
      jq -r '.syscalls[].names[]' "$json_out" >"$syscalls_out" 2>>"$out_dir/go2seccomp.stderr"
      rc_jq=$?
      set -e
      if (( rc_jq != 0 )); then
        echo "ERROR: jq failed with rc=$rc_jq" >> "$out_dir/go2seccomp.stderr"
      fi
    else
      echo "ERROR: go2seccomp json not found: $json_out" >> "$out_dir/go2seccomp.stderr"
    fi
  fi

done

echo
echo "[*] Done."
echo "[*] Results saved in: $OUT_ROOT"
echo "[*] Binaries list:     $OUT_ROOT/_meta/binaries.list"
