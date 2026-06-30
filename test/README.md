# PKE Test Suite

这个目录提供“可直接编译 + 可直接运行”的全量测试脚本，覆盖：

- 功能正确性：`relativepath`、`semaphore`、`cow`、`backtrace`、`wait`、`exec`、`singlepageheap`。
- 诊断与边界：`errorline`（期望触发非法指令诊断）、`sum_sequence`（期望越界失败）、`proc_stress_20/24/28/29`。
- 吞吐/延迟：`fork10/20/28`、`alloc2/alloc4`、`io256x1024`、`pingpong5000`。
- LMbench 风格子集：`lat_proc_fork20`、`lat_proc_exec20`（对应 `lat_proc` 思路，不是 LMbench 全套）。

## 目录说明

- `bench_src/`: 新增 benchmark 的源码（用于生成 `app_bench_*`、`app_lmbench_*`、`app_proc_stress_*`）。
- `build_metrics_bench.sh`: 一键编译（`make all` + 编译新增 benchmark）。
- `run_all_metrics.sh`: 一键运行全量测试并汇总结果。
- `results/`: 每次测试输出目录（按时间戳分目录）。

## 一键编译 + 运行

```bash
bash test/run_all_metrics.sh
```

如果你已经编译过，可跳过编译：

```bash
bash test/run_all_metrics.sh --skip-build
```

## 输出文件

每次运行会在 `test/results/<timestamp>/` 生成：

- `detail.tsv`: 每轮原始记录（用例名、轮次、退出码、是否通过、real 时间、命令）。
- `summary.tsv`: 按指标聚合的成功率和平均耗时。
- `derived_metrics.txt`: 衍生指标（fork/s、ops/s、MB/s、us/op、扩展倍率）。
- `report.md`: 可直接粘贴的测试报告。
- `raw/`: 每轮完整日志。

## 依赖

- `spike`
- `riscv64-unknown-elf-gcc`
- `make`
- `perl`（用于超时保护）

## 指标计算口径

- `fork*_forks_per_s = forks / avg_real_s`
- `alloc2_ops_per_s = 400 / avg_real_s`
- `alloc4_ops_per_s = 800 / avg_real_s`
- `alloc4_vs_alloc2_scale = alloc4_ops_per_s / alloc2_ops_per_s`
- `io_write_MBps = 0.25 MiB / avg_real_s`
- `io_readwrite_MBps = 0.5 MiB / avg_real_s`
- `pingpong_roundtrip_us = avg_real_s * 1e6 / 5000`
- `pingpong_oneway_us = pingpong_roundtrip_us / 2`
- `lat_proc_*_us = avg_real_s * 1e6 / 20`
