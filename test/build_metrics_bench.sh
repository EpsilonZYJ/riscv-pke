#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC_DIR="$ROOT/test/bench_src"
BIN_DIR="$ROOT/hostfs_root/bin"
CC_BIN="${CC:-riscv64-unknown-elf-gcc}"

BASE_CFLAGS=(
  -Wall
  -gdwarf-3
  -fno-builtin
  -nostdlib
  -D__NO_INLINE__
  -mcmodel=medany
  -g
  -Og
  -std=gnu99
  -Wno-unused
  -Wno-attributes
  -fno-delete-null-pointer-checks
  -fno-PIE
  -fno-omit-frame-pointer
  -I"$ROOT"
)

HOST_OS="$(uname -s)"
if [[ "$HOST_OS" == "Darwin" ]]; then
  BASE_CFLAGS+=(-DHOST_MACOS)
elif [[ "$HOST_OS" == "Linux" ]]; then
  BASE_CFLAGS+=(-DHOST_LINUX)
else
  echo "warning: unknown host OS: $HOST_OS, fallback O_CREAT flag may be wrong" >&2
fi

require_cmd() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "missing command: $1" >&2
    exit 1
  fi
}

build_one() {
  local out_name="$1"
  local src_file="$2"
  shift 2

  echo "[build] $out_name"
  "$CC_BIN" "${BASE_CFLAGS[@]}" "$@" --entry=main \
    "$SRC_DIR/$src_file" "$ROOT/user/user_lib.c" "$ROOT/obj/util.a" \
    -o "$BIN_DIR/$out_name"
}

require_cmd make
require_cmd spike
require_cmd "$CC_BIN"

mkdir -p "$BIN_DIR"

echo "[1/3] make all"
make -C "$ROOT" all >/dev/null

if [[ ! -f "$ROOT/obj/util.a" ]]; then
  echo "missing util library: $ROOT/obj/util.a" >&2
  exit 1
fi

echo "[2/3] build benchmark apps in test/bench_src"
build_one app_proc_stress_20 app_proc_stress.c -DTARGET_PROCS=20
build_one app_proc_stress_24 app_proc_stress.c -DTARGET_PROCS=24
build_one app_proc_stress_28 app_proc_stress.c -DTARGET_PROCS=28
build_one app_proc_stress_29 app_proc_stress.c -DTARGET_PROCS=29

build_one app_bench_fork_10 app_bench_fork.c -DTARGET_FORKS=10
build_one app_bench_fork_20 app_bench_fork.c -DTARGET_FORKS=20
build_one app_bench_fork_28 app_bench_fork.c -DTARGET_FORKS=28

build_one app_bench_io_256x1024 app_bench_io.c -DIO_CHUNK_SIZE=256 -DIO_ROUNDS=1024
build_one app_bench_pingpong_5000 app_bench_pingpong.c -DPINGPONG_ITERS=5000

build_one app_lmbench_exec_target app_lmbench_exec_target.c
build_one app_lmbench_lat_proc_fork20 app_lmbench_lat_proc_fork.c -DLATPROC_ITERS=20
build_one app_lmbench_lat_proc_exec20 app_lmbench_lat_proc_exec.c -DLATPROC_ITERS=20

echo "[3/3] done. benchmark binaries are under $BIN_DIR"
