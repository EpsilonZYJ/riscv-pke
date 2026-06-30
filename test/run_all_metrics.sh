#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
KERNEL="$ROOT/obj/riscv-pke"
OUT_BASE="$ROOT/test/results"
BUILD_SCRIPT="$ROOT/test/build_metrics_bench.sh"
IO_HOST_FILE="$ROOT/hostfs_root/io_bench.bin"

cleanup_generated_artifacts() {
  rm -f "$IO_HOST_FILE"
}

trap cleanup_generated_artifacts EXIT

SKIP_BUILD=0
if [[ "${1:-}" == "--skip-build" ]]; then
  SKIP_BUILD=1
fi

if [[ "$SKIP_BUILD" -eq 0 ]]; then
  "$BUILD_SCRIPT"
fi

if [[ ! -x "$KERNEL" ]]; then
  echo "kernel not found: $KERNEL" >&2
  exit 1
fi

ts="$(date +%Y%m%d_%H%M%S)"
out_dir="$OUT_BASE/$ts"
raw_dir="$out_dir/raw"
detail_tsv="$out_dir/detail.tsv"
summary_tsv="$out_dir/summary.tsv"
derived_txt="$out_dir/derived_metrics.txt"
report_md="$out_dir/report.md"

mkdir -p "$raw_dir"

printf "name\trun\texit_code\tok\treal_s\tmode\tcmd\n" > "$detail_tsv"

run_case() {
  local name="$1"
  local runs="$2"
  local mode="$3"
  shift 3
  local cmd_desc="$*"

  for ((i = 1; i <= runs; i++)); do
    local log="$raw_dir/${name}_run${i}.log"

    set +e
    (
      cd "$ROOT" || exit 1
      env LC_ALL=C LANG=C perl -e 'alarm 300; exec @ARGV' \
        /usr/bin/time -p spike -p4 "$KERNEL" "$@" >"$log" 2>&1
    )
    local ec=$?
    set -e

    local real
    real=$(awk '/^real /{v=$2} END{if(v=="") v="nan"; print v}' "$log")

    local ok=0
    case "$mode" in
      ok_exit0)
        if [[ "$ec" -eq 0 ]] \
          && grep -q "System is shutting down with exit code 0\." "$log" \
          && ! grep -q "User exit with code:-1\." "$log"; then
          ok=1
        fi
        ;;
      fail_errorline)
        if [[ "$ec" -ne 0 ]] && grep -q "Runtime error at user/app_errorline.c:13" "$log"; then
          ok=1
        fi
        ;;
      fail_proc29)
        if [[ "$ec" -ne 0 ]] && grep -q "cannot find any free process structure" "$log"; then
          ok=1
        fi
        ;;
      fail_sumseq)
        if [[ "$ec" -ne 0 ]] && grep -q "this address is not available" "$log"; then
          ok=1
        fi
        ;;
      ok_or_proc_overflow)
        if [[ "$ec" -eq 0 ]] && grep -q "\[result\] test finished successfully" "$log"; then
          ok=1
        elif [[ "$ec" -ne 0 ]] && grep -q "cannot find any free process structure" "$log"; then
          ok=1
        fi
        ;;
      *)
        if [[ "$ec" -eq 0 ]]; then
          ok=1
        fi
        ;;
    esac

    printf "%s\t%d\t%d\t%d\t%s\t%s\t%s\n" \
      "$name" "$i" "$ec" "$ok" "$real" "$mode" "$cmd_desc" >> "$detail_tsv"
  done
}

echo "[run] functional metrics"
run_case relativepath 10 ok_exit0 /bin/app_relativepath
run_case semaphore 10 ok_exit0 /bin/app_semaphore
run_case cow 10 ok_exit0 /bin/app_cow
run_case backtrace 10 ok_exit0 /bin/app_print_backtrace
run_case app0 5 ok_exit0 /bin/app0
run_case app1 5 ok_exit0 /bin/app1
run_case app0_app1 10 ok_exit0 /bin/app0 /bin/app1
run_case wait 10 ok_exit0 /bin/app_wait
run_case exec 10 ok_exit0 /bin/app_exec
run_case singlepageheap 10 ok_exit0 /bin/app_singlepageheap
run_case errorline 10 fail_errorline /bin/app_errorline
run_case sum_sequence 10 fail_sumseq /bin/app_sum_sequence

echo "[run] capacity metrics"
run_case proc20 10 ok_exit0 /bin/app_proc_stress_20
run_case proc24 10 ok_exit0 /bin/app_proc_stress_24
run_case proc28 10 ok_exit0 /bin/app_proc_stress_28
run_case proc29 10 fail_proc29 /bin/app_proc_stress_29

echo "[run] throughput and latency metrics"
run_case fork10 10 ok_exit0 /bin/app_bench_fork_10
run_case fork20 10 ok_exit0 /bin/app_bench_fork_20
run_case fork28 10 ok_exit0 /bin/app_bench_fork_28
run_case alloc2 5 ok_exit0 /bin/app_alloc0 /bin/app_alloc1
run_case alloc4 5 ok_exit0 /bin/app_alloc0 /bin/app_alloc1 /bin/app_alloc0 /bin/app_alloc1
run_case io256x1024 5 ok_exit0 /bin/app_bench_io_256x1024
run_case pingpong5000 5 ok_exit0 /bin/app_bench_pingpong_5000
run_case latproc_fork20 10 ok_exit0 /bin/app_lmbench_lat_proc_fork20
run_case latproc_exec20 10 ok_exit0 /bin/app_lmbench_lat_proc_exec20
run_case perf_mix 5 ok_or_proc_overflow /bin/app_test_performance

printf "name\tsuccess\ttotal\tavg_real_s\n" > "$summary_tsv"
for name in \
  relativepath semaphore cow backtrace app0 app1 app0_app1 wait exec singlepageheap errorline sum_sequence \
  proc20 proc24 proc28 proc29 \
  fork10 fork20 fork28 \
  alloc2 alloc4 io256x1024 pingpong5000 latproc_fork20 latproc_exec20 perf_mix
  do
  awk -F '\t' -v n="$name" 'NR>1 && $1==n {total++; succ+=$4; if($5!="nan"){sum+=$5; c++}} END{if(c==0){avg="nan"} else {avg=sprintf("%.6f", sum/c)}; printf "%s\t%d\t%d\t%s\n", n, succ+0, total+0, avg}' "$detail_tsv" >> "$summary_tsv"
done

avg_of() {
  awk -F '\t' -v n="$1" '$1==n{print $4}' "$summary_tsv"
}

avg_f10="$(avg_of fork10)"
avg_f20="$(avg_of fork20)"
avg_f28="$(avg_of fork28)"
avg_a2="$(avg_of alloc2)"
avg_a4="$(avg_of alloc4)"
avg_io="$(avg_of io256x1024)"
avg_pp="$(avg_of pingpong5000)"
avg_lf="$(avg_of latproc_fork20)"
avg_le="$(avg_of latproc_exec20)"

{
  echo "[derived]"
  echo "fork10_forks_per_s=$(awk -v t="$avg_f10" 'BEGIN{if(t>0) printf "%.2f", 10.0/t; else print "nan"}')"
  echo "fork20_forks_per_s=$(awk -v t="$avg_f20" 'BEGIN{if(t>0) printf "%.2f", 20.0/t; else print "nan"}')"
  echo "fork28_forks_per_s=$(awk -v t="$avg_f28" 'BEGIN{if(t>0) printf "%.2f", 28.0/t; else print "nan"}')"
  echo "alloc2_ops_per_s=$(awk -v t="$avg_a2" 'BEGIN{if(t>0) printf "%.2f", 400.0/t; else print "nan"}')"
  echo "alloc4_ops_per_s=$(awk -v t="$avg_a4" 'BEGIN{if(t>0) printf "%.2f", 800.0/t; else print "nan"}')"
  echo "alloc4_vs_alloc2_scale=$(awk -v a="$avg_a2" -v b="$avg_a4" 'BEGIN{if(a>0 && b>0) printf "%.3f", (800.0/b)/(400.0/a); else print "nan"}')"
  echo "io_write_MBps=$(awk -v t="$avg_io" 'BEGIN{if(t>0) printf "%.3f", (262144.0/1048576.0)/t; else print "nan"}')"
  echo "io_readwrite_MBps=$(awk -v t="$avg_io" 'BEGIN{if(t>0) printf "%.3f", (524288.0/1048576.0)/t; else print "nan"}')"
  echo "pingpong_roundtrip_us=$(awk -v t="$avg_pp" 'BEGIN{if(t>0) printf "%.2f", (t*1000000.0)/5000.0; else print "nan"}')"
  echo "pingpong_oneway_us=$(awk -v t="$avg_pp" 'BEGIN{if(t>0) printf "%.2f", (t*1000000.0)/(5000.0*2.0); else print "nan"}')"
  echo "lat_proc_fork_us=$(awk -v t="$avg_lf" 'BEGIN{if(t>0) printf "%.2f", (t*1000000.0)/20.0; else print "nan"}')"
  echo "lat_proc_exec_us=$(awk -v t="$avg_le" 'BEGIN{if(t>0) printf "%.2f", (t*1000000.0)/20.0; else print "nan"}')"
} > "$derived_txt"

{
  echo "# PKE Metrics Report"
  echo
  echo "- date: $(date '+%Y-%m-%d %H:%M:%S %Z')"
  echo "- kernel: $KERNEL"
  echo "- output_dir: $out_dir"
  echo
  echo "## Summary"
  echo
  echo '```tsv'
  cat "$summary_tsv"
  echo '```'
  echo
  echo "## Derived"
  echo
  echo '```txt'
  cat "$derived_txt"
  echo '```'
} > "$report_md"

echo "OUT_DIR=$out_dir"
echo "DETAIL=$detail_tsv"
echo "SUMMARY=$summary_tsv"
echo "DERIVED=$derived_txt"
echo "REPORT=$report_md"
