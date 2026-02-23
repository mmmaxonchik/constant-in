#!/usr/bin/env bash

set -euo pipefail

TEST="${1:-}"
if [[ -z "${TEST}" ]]; then
  echo "Usage: $0 <testname> [-- <args>]" >&2
  exit 2
fi
shift || true

PROG_ARGS=()
if [[ "${1:-}" == "--" ]]; then
  shift
  PROG_ARGS=("$@")
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="${ROOT_DIR}/src"
BUILD_ROOT="${ROOT_DIR}/build"

OUT_ROOT="${BUILD_ROOT}/${TEST}"
mkdir -p "${OUT_ROOT}"

MANIFEST="${OUT_ROOT}/manifest.txt"
: > "${MANIFEST}"

log() { echo "$*" | tee -a "${MANIFEST}"; }
have() { command -v "$1" >/dev/null 2>&1; }

export LANG=C
export LC_ALL=C
export TZ=UTC

FAIL_ON_BUILD_ERROR="${FAIL_ON_BUILD_ERROR:-1}"

TIMEOUT_SECS="${TIMEOUT_SECS:-5}"
TRACE_ENV=(env LANG=C LC_ALL=C TZ=UTC)

strip_copy() {
  local in="$1" out="$2"
  if have strip; then
    cp -f "$in" "$out"
    strip "$out" 2>/dev/null || true
    log "    + stripped: $(basename "$out")"
  else
    log "    ! strip not found; skipped stripped copy for $(basename "$in")"
  fi
}

is_elf_x86_64() {
  local f="$1"
  if have file; then
    file "$f" | grep -qE 'ELF 64-bit.*x86-64'
  else
    return 0
  fi
}

collect_syscalls_for_binary() {
  local bin="$1"
  local out_dir="$2"
  local base="$3"

  local trace_log="${out_dir}/${base}.strace"
  local syscalls_out="${out_dir}/${base}.syscalls"
  local run_rc="${out_dir}/${base}.run.rc"

  if ! have strace; then
    log "    ! strace not found; syscall ground truth skipped for $(basename "$bin")"
    return 0
  fi
  if ! have timeout; then
    log "    ! timeout not found; syscall ground truth skipped for $(basename "$bin")"
    return 0
  fi

  log "    * tracing: $(basename "$bin") -> $(basename "$trace_log")"
  : > "$trace_log"

  set +e
  "${TRACE_ENV[@]}" timeout "${TIMEOUT_SECS}" \
    strace -f -qq -e trace=all -s 256 -o "$trace_log" -- "$bin" "${PROG_ARGS[@]}" >/dev/null 2>&1
  local rc=$?
  set -e
  echo "$rc" > "$run_rc"

  awk '
    {
      if (match($0, /^\[pid [0-9]+\] ([A-Za-z_][A-Za-z0-9_]*)\(/, m)) { print m[1]; next }
      if (match($0, /^[0-9]+[[:space:]]+([A-Za-z_][A-Za-z0-9_]*)\(/, m)) { print m[1]; next }
      if (match($0, /^([A-Za-z_][A-Za-z0-9_]*)\(/, m)) { print m[1]; next }
    }
  ' "$trace_log" | sort -u > "$syscalls_out"

  log "    + syscalls: $(basename "$syscalls_out") ($(wc -l < "$syscalls_out" | tr -d " ") entries)"
}

build_and_trace() {
  local bin="$1"
  local out_dir="$2"

  local base
  base="$(basename "$bin")"

  if [[ ! -x "$bin" ]]; then
    log "    ! not executable: $base"
    return 0
  fi

  if ! is_elf_x86_64 "$bin"; then
    log "    ! not ELF x86-64 (skipping trace): $base"
    return 0
  fi

  collect_syscalls_for_binary "$bin" "$out_dir" "$base"

  strip_copy "$bin" "${bin}_strip"
  if [[ -x "${bin}_strip" ]]; then
    collect_syscalls_for_binary "${bin}_strip" "$out_dir" "${base}_strip"
  fi
}

build_c_family() {
  local lang="$1"
  local compiler="$2"
  local src="$3"
  local out_dir="$4"

  local common_flags=()
 if [[ "$lang" == "c" ]]; then
    common_flags=(-std=c11 -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE)
  else
    common_flags=(-std=c++17)
  fi

  local opt_flags_list=(
    "O0:-O0 -g"
    "O2:-O2"
  )

  local lto_flags_list=(
    "nolto:"
    "lto:-flto"
  )

  local pie_flags_list=(
    "pie:-fPIE -pie"
    "nopie:-fno-PIE -no-pie"
  )

  local link_mode_list=(
    "dyn:"
    "static:-static"
  )
  if [[ "${TEST}" == "t08_shared_so" || "${TEST}" == "t10_many_in_so" ]]; then
    link_mode_list=("dyn:")
  fi

  log "  --- ${lang^^} builds (${compiler}) ---"

  for opt_item in "${opt_flags_list[@]}"; do
    local opt_tag="${opt_item%%:*}"
    local opt_flags="${opt_item#*:}"

    for lto_item in "${lto_flags_list[@]}"; do
      local lto_tag="${lto_item%%:*}"
      local lto_flags="${lto_item#*:}"

      for pie_item in "${pie_flags_list[@]}"; do
        local pie_tag="${pie_item%%:*}"
        local pie_flags="${pie_item#*:}"

        for link_item in "${link_mode_list[@]}"; do
          local link_tag="${link_item%%:*}"
          local link_flags="${link_item#*:}"

          local out="${out_dir}/${TEST}_${lang}_${compiler}_${opt_tag}_${lto_tag}_${pie_tag}_${link_tag}"
          local blog="${out}.build.log"

          local extra_link_flags=""
          if [[ "${TEST}" == "t08_shared_so" ]]; then
            extra_link_flags="-ldl"
          elif [[ "${TEST}" == "t10_many_in_so" ]]; then
            extra_link_flags="-L${out_dir} -Wl,-rpath,\$ORIGIN -lt10_syswrap"
          elif [[ "${TEST}" == "t09_static_lib" ]]; then
            extra_link_flags="-Wl,--whole-archive ${out_dir}/libt09_syswrap.a -Wl,--no-whole-archive"
          fi

          log "    * build: $(basename "$out")"
          set +e
          "$compiler" "${common_flags[@]}" $opt_flags $lto_flags $pie_flags "$src" -o "$out" $link_flags $extra_link_flags >"$blog" 2>&1
          local rc=$?
          set -e
          if [[ $rc -ne 0 ]]; then
            log "      - failed (see $(basename "$blog"))"
            rm -f "$out" 2>/dev/null || true

            if [[ "$FAIL_ON_BUILD_ERROR" -eq 1 ]]; then
              log "      ! stopping due to build failure"
              return 1
            fi

            continue
          fi

          build_and_trace "$out" "$out_dir"
        done
      done
    done
  done
}

build_go() {
  local src="$1"
  local out_dir="$2"

  log "  --- Go builds ---"
  if ! have go; then
    log "    ! go not found; skipping"
    return 0
  fi

  local uses_cgo="no"
  if grep -qE '^[[:space:]]*import[[:space:]]+"C"' "$src"; then
    uses_cgo="yes"
  fi

  local cgo_ldflags_extra=""
  if [[ "${TEST}" == "t09_static_lib" ]]; then
    cgo_ldflags_extra="-Wl,--whole-archive ${out_dir}/libt09_syswrap.a -Wl,--no-whole-archive"
  elif [[ "${TEST}" == "t10_many_in_so" ]]; then
    cgo_ldflags_extra="-L${out_dir} -Wl,-rpath,\$ORIGIN -lt10_syswrap"
  fi

  local buildmodes=("exe" "pie")
  local variant_tags=("rel" "dbg" "strip")

  for bm in "${buildmodes[@]}"; do
    for v_tag in "${variant_tags[@]}"; do
      local flags=()
      if [[ "$v_tag" == "dbg" ]]; then
        flags+=(-gcflags "all=-N -l")
      elif [[ "$v_tag" == "strip" ]]; then
        flags+=(-ldflags "-s -w")
      fi

      if [[ "$uses_cgo" == "yes" ]]; then
        local out="${out_dir}/${TEST}_go_cgo_${bm}_${v_tag}"
        local blog="${out}.build.log"
        log "    * build: $(basename "$out")"
        set +e
        CGO_ENABLED=1 CGO_LDFLAGS="${cgo_ldflags_extra}" go build -buildmode="$bm" "${flags[@]}" -o "$out" "$src" >"$blog" 2>&1
        local rc=$?
        set -e
        if [[ $rc -ne 0 ]]; then
          log "      - failed (see $(basename "$blog"))"
          rm -f "$out" 2>/dev/null || true

          if [[ "$FAIL_ON_BUILD_ERROR" -eq 1 ]]; then
            log "      ! stopping due to build failure"
            return 1
          fi

          continue
        fi
        build_and_trace "$out" "$out_dir"

      else
        local out1="${out_dir}/${TEST}_go_cgo_enabled_${bm}_${v_tag}"
        local blog1="${out1}.build.log"
        log "    * build: $(basename "$out1")"
        set +e
        CGO_ENABLED=1 CGO_LDFLAGS="${cgo_ldflags_extra}" go build -buildmode="$bm" "${flags[@]}" -o "$out1" "$src" >"$blog1" 2>&1
        local rc1=$?
        set -e
        if [[ $rc1 -eq 0 ]]; then
          build_and_trace "$out1" "$out_dir"
        else
          log "      - failed (see $(basename "$blog1"))"
          rm -f "$out1" 2>/dev/null || true

          if [[ "$FAIL_ON_BUILD_ERROR" -eq 1 ]]; then
            log "      ! stopping due to build failure"
            return 1
          fi
        fi

        local out2="${out_dir}/${TEST}_go_pure_${bm}_${v_tag}"
        local blog2="${out2}.build.log"
        log "    * build: $(basename "$out2")"
        set +e
        CGO_ENABLED=0 go build -buildmode="$bm" "${flags[@]}" -o "$out2" "$src" >"$blog2" 2>&1
        local rc2=$?
        set -e
        if [[ $rc2 -eq 0 ]]; then
          build_and_trace "$out2" "$out_dir"
        else
          log "      - failed (see $(basename "$blog2"))"
          rm -f "$out2" 2>/dev/null || true

          if [[ "$FAIL_ON_BUILD_ERROR" -eq 1 ]]; then
            log "      ! stopping due to build failure"
            return 1
          fi
        fi
      fi
    done
  done
}

build_rust() {
  local src="$1"
  local out_dir="$2"

  log "  --- Rust builds ---"
  if ! have rustc; then
    log "    ! rustc not found; skipping"
    return 0
  fi

  local opt_list=(
    "O0:-C opt-level=0 -g"
    "O3:-C opt-level=3"
  )
  local lto_list=(
    "nolto:"
    "lto:-C lto=yes"
  )
  local pie_list=(
    "pie:-C link-arg=-pie"
    "nopie:-C link-arg=-no-pie"
  )

  for opt_item in "${opt_list[@]}"; do
    local opt_tag="${opt_item%%:*}"
    local opt_flags="${opt_item#*:}"

    for lto_item in "${lto_list[@]}"; do
      local lto_tag="${lto_item%%:*}"
      local lto_flags="${lto_item#*:}"

      for pie_item in "${pie_list[@]}"; do
        local pie_tag="${pie_item%%:*}"
        local pie_flags="${pie_item#*:}"

        local out="${out_dir}/${TEST}_rust_${opt_tag}_${lto_tag}_${pie_tag}_dyn"
        local blog="${out}.build.log"
        local extra_link_args=()
        if [[ "${TEST}" == "t08_shared_so" ]]; then
          extra_link_args=(-C link-arg=-ldl)
        elif [[ "${TEST}" == "t10_many_in_so" ]]; then
          extra_link_args=(-L "native=${out_dir}" -l "dylib=t10_syswrap" -C link-arg=-Wl,-rpath,\$ORIGIN)
        elif [[ "${TEST}" == "t09_static_lib" ]]; then
          extra_link_args=(-L "native=${out_dir}" -l "static=t09_syswrap")
        fi

        log "    * build: $(basename "$out")"
        set +e
        rustc $opt_flags $lto_flags $pie_flags "${extra_link_args[@]}" "$src" -o "$out" >"$blog" 2>&1
        local rc=$?
        set -e
        if [[ $rc -ne 0 ]]; then
          log "      - failed (see $(basename "$blog"))"
          rm -f "$out" 2>/dev/null || true

          if [[ "$FAIL_ON_BUILD_ERROR" -eq 1 ]]; then
            log "      ! stopping due to build failure"
            return 1
          fi

          continue
        fi
        build_and_trace "$out" "$out_dir"
      done
    done
  done

  if rustc --print target-list 2>/dev/null | grep -q '^x86_64-unknown-linux-musl$'; then
    local out="${out_dir}/${TEST}_rust_O3_musl_static"
    local blog="${out}.build.log"
    local extra_link_args=()
    if [[ "${TEST}" == "t08_shared_so" ]]; then
      extra_link_args=(-C link-arg=-ldl)
    elif [[ "${TEST}" == "t10_many_in_so" ]]; then
      extra_link_args=(-L "native=${out_dir}" -l "dylib=t10_syswrap" -C link-arg=-Wl,-rpath,\$ORIGIN)
    elif [[ "${TEST}" == "t09_static_lib" ]]; then
      extra_link_args=(-L "native=${out_dir}" -l "static=t09_syswrap")
    fi

    log "    * build: $(basename "$out")"
    set +e
    rustc -C opt-level=3 -C lto=yes --target x86_64-unknown-linux-musl "${extra_link_args[@]}" "$src" -o "$out" >"$blog" 2>&1    local rc=$?
    set -e
    if [[ $rc -eq 0 ]]; then
      build_and_trace "$out" "$out_dir"
    else
      log "      - failed (see $(basename "$blog"))"
      rm -f "$out" 2>/dev/null || true

      if [[ "$FAIL_ON_BUILD_ERROR" -eq 1 ]]; then
        log "      ! stopping due to build failure"
        return 1
      fi
    fi
  else
    log "    ! musl target not available; Rust static variant skipped"
  fi
}


build_shared_t08_if_needed() {
  local out_dir="$1"
  if [[ "${TEST}" != "t08_shared_so" ]]; then
    return 0
  fi

  local helper_src="${SRC_DIR}/shared/t08_syswrap.c"
  local helper_out="${out_dir}/libt08_syswrap.so"

  if [[ ! -f "$helper_src" ]]; then
    log "  ! t08 helper source not found: $helper_src"
    return 1
  fi
  if ! have gcc; then
    log "  ! gcc not found; cannot build t08 helper .so"
    return 1
  fi

  log "  * build helper .so: $(realpath --relative-to="$OUT_ROOT" "$helper_out" 2>/dev/null || basename "$helper_out")"
  gcc -shared -fPIC -O2 "$helper_src" -o "$helper_out"
}

build_shared_t10_if_needed() {
  local out_dir="$1"
  if [[ "${TEST}" != "t10_many_in_so" ]]; then
    return 0
  fi

  local helper_src="${SRC_DIR}/shared/t10_syswrap.c"
  local helper_out="${out_dir}/libt10_syswrap.so"

  if [[ ! -f "$helper_src" ]]; then
    log "  ! t10 helper source not found: $helper_src"
    return 1
  fi
  if ! have gcc; then
    log "  ! gcc not found; cannot build t10 helper .so"
    return 1
  fi

  log "  * build helper .so: $(realpath --relative-to="$OUT_ROOT" "$helper_out" 2>/dev/null || basename "$helper_out")"
  gcc -shared -fPIC -O2 -nostdlib -nodefaultlibs "$helper_src" -o "$helper_out"
}

build_static_t09_if_needed() {
  local out_dir="$1"
  if [[ "${TEST}" != "t09_static_lib" ]]; then
    return 0
  fi

  local helper_src="${SRC_DIR}/shared/t09_syswrap.c"
  local helper_obj="${out_dir}/t09_syswrap.o"
  local helper_out="${out_dir}/libt09_syswrap.a"

  if [[ ! -f "$helper_src" ]]; then
    log "  ! t09 helper source not found: $helper_src"
    return 1
  fi
  if ! have gcc; then
    log "  ! gcc not found; cannot build t09 helper .a"
    return 1
  fi
  if ! have ar; then
    log "  ! ar not found; cannot build t09 helper .a"
    return 1
  fi

  log "  * build helper .a: $(realpath --relative-to="$OUT_ROOT" "$helper_out" 2>/dev/null || basename "$helper_out")"
  gcc -O2 -fPIC -c "$helper_src" -o "$helper_obj"
  ar rcs "$helper_out" "$helper_obj"
}

log "=== Test: ${TEST} (args: ${PROG_ARGS[*]-}) ==="
log "=== Looking for sources in: ${SRC_DIR}/{c,cpp,go,rust} ==="

built_any=0

c_src="${SRC_DIR}/c/${TEST}.c"
if [[ -f "$c_src" ]]; then
  built_any=1
  c_out="${OUT_ROOT}/c"
  mkdir -p "$c_out"
  build_shared_t08_if_needed "$c_out"
  build_shared_t10_if_needed "$c_out"
  build_static_t09_if_needed "$c_out"
  log "== Found C: $c_src -> $c_out =="
  if have gcc; then build_c_family "c" "gcc" "$c_src" "$c_out"; else log "  ! gcc not found; C(gcc) skipped"; fi
  if have clang; then build_c_family "c" "clang" "$c_src" "$c_out"; else log "  ! clang not found; C(clang) skipped"; fi
fi

cpp_src="${SRC_DIR}/cpp/${TEST}.cpp"
if [[ -f "$cpp_src" ]]; then
  built_any=1
  cpp_out="${OUT_ROOT}/cpp"
  mkdir -p "$cpp_out"
  build_shared_t08_if_needed "$cpp_out"
  build_shared_t10_if_needed "$cpp_out"
  build_static_t09_if_needed "$cpp_out"
  log "== Found C++: $cpp_src -> $cpp_out =="
  if have g++; then build_c_family "cpp" "g++" "$cpp_src" "$cpp_out"; else log "  ! g++ not found; C++(g++) skipped"; fi
  if have clang++; then build_c_family "cpp" "clang++" "$cpp_src" "$cpp_out"; else log "  ! clang++ not found; C++(clang++) skipped"; fi
fi

go_src="${SRC_DIR}/go/${TEST}.go"
if [[ -f "$go_src" ]]; then
  built_any=1
  go_out="${OUT_ROOT}/go"
  mkdir -p "$go_out"
  build_shared_t08_if_needed "$go_out"
  build_shared_t10_if_needed "$go_out"
  build_static_t09_if_needed "$go_out"
  log "== Found Go: $go_src -> $go_out =="
  build_go "$go_src" "$go_out"
fi

rs_src="${SRC_DIR}/rust/${TEST}.rs"
if [[ -f "$rs_src" ]]; then
  built_any=1
  rs_out="${OUT_ROOT}/rust"
  mkdir -p "$rs_out"
  build_shared_t08_if_needed "$rs_out"
  build_shared_t10_if_needed "$rs_out"
  build_static_t09_if_needed "$rs_out"
  log "== Found Rust: $rs_src -> $rs_out =="
  build_rust "$rs_src" "$rs_out"
fi

if [[ "$built_any" -eq 0 ]]; then
  log "ERROR: No sources found for '${TEST}' in src/{c,cpp,go,rust}"
  exit 1
fi

summarize_dir() {
  local dir="$1"
  local out="$2"
  if compgen -G "${dir}/*.syscalls" > /dev/null; then
    cat "${dir}"/*.syscalls | sort -u > "$out"
    log "  + summary: $(realpath --relative-to="$OUT_ROOT" "$out" 2>/dev/null || basename "$out") ($(wc -l < "$out" | tr -d ' ') entries)"
  fi
}

log "=== Writing syscall summaries ==="
summarize_dir "${OUT_ROOT}/c"    "${OUT_ROOT}/${TEST}.c.all.syscalls"    || true
summarize_dir "${OUT_ROOT}/cpp"  "${OUT_ROOT}/${TEST}.cpp.all.syscalls"  || true
summarize_dir "${OUT_ROOT}/go"   "${OUT_ROOT}/${TEST}.go.all.syscalls"   || true
summarize_dir "${OUT_ROOT}/rust" "${OUT_ROOT}/${TEST}.rust.all.syscalls" || true

if compgen -G "${OUT_ROOT}/*/*.syscalls" > /dev/null; then
  cat "${OUT_ROOT}"/*/*.syscalls | sort -u > "${OUT_ROOT}/${TEST}.all.syscalls"
  log "  + summary: ${TEST}.all.syscalls ($(wc -l < "${OUT_ROOT}/${TEST}.all.syscalls" | tr -d ' ') entries)"
fi

log "=== Done. Artifacts are in ${OUT_ROOT} ==="
