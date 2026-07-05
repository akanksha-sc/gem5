# SALAM gem5 standard library launch path

This directory contains the stdlib replacement for generated
`configs/SALAM/fs_<bench>.py` launchers.

## What the stdlib path does

The stdlib SALAM path replaces **per-benchmark** full-system scripts
`fs_<bench>.py` with a **single** runner (`run_salam_stdlib.py`) and a
`SALAMArmBoard` adapter that exposes the legacy `system` object surface
expected by generated `configs/SALAM/<bench>.py`.

The **accelerator topology** is still produced by
`util/SALAM-tools/SALAM-Configurator/systembuilder.py` and
`config_parser.py`; the entry point remains
`makeHWAcc(args, system)`.

## Core flow

```text
run_system_stdlib.sh
  -> systembuilder.py --no-fs-template [--emit-manifest]
  -> generated configs/SALAM/<bench>.py (+ headers, optional .manifest.yml)
  -> make all -C $ACC_BENCH_PATH/$BENCH_PATH  (build sw/main.elf)
  -> gem5.opt run_salam_stdlib.py
  -> SALAMArmBoard
  -> import <bench>  # from configs/SALAM/
  -> makeHWAcc(args, board)
  -> Simulator(board=board).run()
```

## Supported configurations

- **ISA:** ARM
- **Workload:** bare-metal ELF (`--kernel` → `sw/main.elf`)
- **Platform:** `VExpress_GEM5_V1` (default in the runner)
- **Memory / cache:** stdlib `DualChannelDDR4_2400` + classic
  `PrivateL1PrivateL2CacheHierarchy`
- **Accelerators:** generated `configs/SALAM/<bench>.py` only
- **Coherency:** current generated configs use
  `clstr._connect_caches(..., l2coherent=False)` and attach through
  `system.membus` compatibility aliases

## Not supported yet

- Ruby memory system
- Linux kernel + disk-image workloads (`set_kernel_disk_workload`)
- True L2-coherent accelerator attachment (beyond `l2coherent=False` +
  `membus` alias)
- Separate per-CPU clock in the runner (only `--sys-clock` for the board)

This path **does not** pass `--disk-image`. Legacy `run_system.sh` may pass a
fake ISO for full-system templates; stdlib SALAM is bare-metal only.

## Compatibility contract

`SALAMArmBoard` exposes (for generated code on `system` / `board`):

- `iobus`
- `membus` (alias to `cache_hierarchy.membus`)
- `tol2bus` (currently aliased to `membus` for `l2coherent=False` configs)
- `realview.gic`
- `clk_domain`
- `acc_compute_clk_domain`, `acc_mem_clk_domain`, `acc_dma_clk_domain`,
  `acc_localbus_clk_domain`
- Legacy aliases: `acc_clk_domain`, `acc_voltage_domain` (compute domain)

## Prerequisites

- Build gem5 with ARM and SALAM after adding new stdlib Python modules
  (e.g. `salam_arm_board.py` must be listed in `src/python/SConscript`).
- Set environment variables:

  ```bash
  export M5_PATH=/path/to/gem5
  export ACC_BENCH_PATH=/path/to/benchmarks
  ```

## Example: full run with metrics

```bash
$M5_PATH/util/SALAM-tools/run_system_stdlib.sh \
  --bench gemm \
  --bench-path sys_validation/gemm \
  --outdir BM_ARM_OUT/stdlib/gemm \
  --test-metrics
```

## Example: dry-run (attach only)

```bash
$M5_PATH/util/SALAM-tools/run_system_stdlib.sh \
  --bench gemm \
  --bench-path sys_validation/gemm \
  --outdir BM_ARM_OUT/dryrun/gemm \
  --dry-run
```

Or call gem5 directly after running `systembuilder.py` and building the ELF.

## What dry-run proves

**Proves:**

- Generated `configs/SALAM/<bench>.py` imports
- Stdlib ARM board constructs (RealView, GIC, iobus, memory ranges)
- Compatibility aliases exist
- `makeHWAcc(args, board)` runs and attaches SALAM objects

**Does not prove:**

- Guest ELF runs correctly
- MMIO reaches accelerators
- DMA / memory traffic is correct
- Benchmark functional correctness

## Generator: legacy vs stdlib mode

**Default** (`systembuilder.py` without `--no-fs-template`):

- Emits `configs/SALAM/<bench>.py`, `fs_<bench>.py`, and headers.

**Stdlib** (`--no-fs-template`, optionally `--emit-manifest`):

- Emits `<bench>.py` and headers; skips `fs_<bench>.py`.
- Optional `configs/SALAM/<bench>.manifest.yml` for tooling (informational).

## Comparing legacy vs stdlib SALAM summaries

After capturing `debug-trace.txt` from both flows:

```bash
python3 $M5_PATH/util/SALAM-tools/compare_salam_summary.py \
  --legacy BM_ARM_OUT/legacy/gemm/debug-trace.txt \
  --stdlib BM_ARM_OUT/stdlib/gemm/debug-trace.txt
```

Optional tolerances: `--runtime-tol`, `--compute-tol`, `--memory-tol`.

Positional invocation (`legacy debug-trace.txt` then `stdlib debug-trace.txt`)
is still accepted for backward compatibility.

## Validation sweep

From `$M5_PATH`:

```bash
util/SALAM-tools/validate_stdlib.sh              # full stdlib + test-metrics
util/SALAM-tools/validate_stdlib.sh --dry-run
util/SALAM-tools/validate_stdlib.sh --mobilenet  # add MobileNetV2 configs
```

## Note on `--emit-legacy-fs`

An earlier prototype exposed a no-op `--emit-legacy-fs` flag; it was removed.
Legacy FS emission is implied whenever `--no-fs-template` is **not** passed.
