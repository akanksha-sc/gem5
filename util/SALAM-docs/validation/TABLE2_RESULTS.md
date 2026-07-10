# SALAM Table 2 Validation Results

**Generated:** 2026-07-07 19:16 UTC
**Branch:** `gem5-salam-develop-clocked-working`
**Reference:** Spencer et al., *Journal of Systems Architecture* 154 (2024) 103211, Table 2 (gem5-SALAMv2 column, 40 nm, 10 ns profile)

Canonical traces: `validation_results/<bench>/debug-trace.txt` (from `validation_salam.py` at 100 MHz accelerator / 500 MHz SPM). `stencil3d` cycles use `BM_ARM_OUT/post_regfix/stencil3d` because the `validation_results/stencil3d` trace predates `--paper-timing`.

## Summary (core power — Table 2 datapath metric)

Paper Table 2 **power** is the **core** row (FU dynamic + leakage + register file). **Cycles** use `runtime_cycles` from `SALAM_SUMMARY` except **md_knn**, where paper reports `runtime_cycles × 5` (double-precision multiplier pipeline depth). **Area** is FU area (µm²).

| Benchmark | Sim cycles | Paper cycles | Cycle err% | Sim core (mW) | Paper core (mW) | Power err% | Sim area (µm²) | Paper area (µm²) | Area err% | Status |
|-----------|------------|--------------|------------|---------------|-----------------|------------|----------------|-------------------|-----------|--------|
| bfs | 15,600 | 15,600 | +0.00% | 1.3487 | 1.3497 | -0.07% | 7725.5 | 7696.0 | +0.38% | ✅ |
| fft | 86,035 | 91,265 | -5.73% | 59.1313 | 59.3513 | -0.37% | 38533.0 | 39228.0 | -1.77% | ⚠️ |
| gemm | 131,462 | 131,900 | -0.33% | 63.9037 | 65.3655 | -2.24% | 273003.2 | 289400.0 | -5.67% | ⚠️ |
| md_knn | 328,035 | 328,025 | +0.00% | 14.7616 | 15.9594 | -7.51% | 42121.9 | 42791.0 | -1.56% | ⚠️ |
| nw | 66,963 | 66,962 | +0.00% | 6.5057 | 6.1975 | +4.97% | 12183.9 | 12152.0 | +0.26% | ✅ |
| stencil2d | 109,563 | 109,563 | +0.00% | 44.5424 | 43.4334 | +2.55% | 10239.0 | 9000.0 | +13.77% | ❌ |
| stencil3d | 47,391 | 47,210 | +0.38% | 2.0224 | — | — | 288598.1 | — | — | ✅ |

**Benchmarks within 5% on all reported metrics:** bfs, nw, stencil3d

## SPM-inclusive power (memory-heavy kernels)

For fft/gemm/stencil*, paper values historically matched **SPM-inclusive** power when CACTI scratchpad energy is included. Core-only comparison is misleading for those workloads.

| Benchmark | SPM-inclusive (mW) | Paper core ref (mW) | Err% |
|-----------|-------------------|---------------------|------|
| bfs | 31.2192 | 1.3497 | +2213.05% |
| fft | 173.6246 | 59.3513 | +192.54% |
| gemm | 326.4544 | 65.3655 | +399.43% |
| md_knn | 55.9698 | 15.9594 | +250.70% |
| nw | 134.9436 | 6.1975 | +2077.39% |
| stencil2d | 188.7371 | 43.4334 | +334.54% |
| stencil3d | 141.7353 | — | — |

## sys_validation extended benchmarks (no paper reference)

| Benchmark | Sim cycles | Core (mW) | SPM-inclusive (mW) | FU area (µm²) | Trace |
|-----------|------------|-----------|----------------------|---------------|-------|
| md_grid | 333,634 | 14.8330 | 109.5164 | 0.0 | `validation_results/md_grid` |
| mergesort | 202,741 | 0.7798 | 112.4785 | 4810.6 | `validation_results/mergesort` |
| spmv | 8,348 | 18.7130 | 42.9901 | 0.0 | `validation_results/spmv` |

## Trace sources

| Benchmark | Directory |
|-----------|-----------|
| bfs | `validation_results/bfs` |
| fft | `validation_results/fft` |
| gemm | `validation_results/gemm` |
| md_knn | `validation_results/md_knn` |
| nw | `validation_results/nw` |
| stencil2d | `validation_results/stencil2d` |
| stencil3d | `BM_ARM_OUT/post_regfix/stencil3d` |

## Why older `VALIDATION_RESULTS.md` showed high errors

A July 4 report compared **weighted_cycles** (or top-level `Runtime:`) instead of kernel `runtime_cycles`, and sometimes read power from the wrong accelerator block. Re-parsing with `compare_table2.py` / `validation_salam.py --skip-run` recovers the numbers in this table.

## Reproduce

```bash
# Regenerate this document
python3 util/SALAM-tools/generate_validation_report.py \
  --m5-path /path/to/gem5

# Re-run Table 2 sims (6 benchmarks)
python3 util/SALAM-tools/validation_salam.py \
  --m5-path /path/to/gem5 \
  --acc-bench-path /path/to/benchmarks/sys_validation

# Compare arbitrary trace dirs
python3 util/SALAM-tools/compare_table2.py validation_results/bfs
```
