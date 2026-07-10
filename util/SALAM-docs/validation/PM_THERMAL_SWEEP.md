# PM/Thermal Benchmark Sweep

**Output:** `m5out_pm_thermal_sweep/`
**Script:** `util/SALAM-tools/sweep_pm_thermal_traces.sh`

Full-system SALAM benchmarks run with interval **power** and **thermal**
sampling plus area-proportional HotSpot floorplans (Phase 5b).

---

## Sweep summary (2026-07-07)

| Benchmark | Status | Power rows | Thermal rows | Floorplan |
|-----------|--------|------------|--------------|-----------|
| bfs | OK | 84 | 84 | yes |
| fft | OK | 1344 | 1344 | yes |
| gemm | OK | 474 | 474 | yes |
| md_knn | OK | 156 | 156 | yes |
| md_grid | OK | 978 | 978 | yes |
| mergesort | OK | 2952 | 2952 | yes |
| nw | OK | 438 | 438 | yes |
| spmv | OK | 846 | 846 | yes |
| stencil2d | OK | 13776 | 13776 | yes |
| stencil3d | OK | 12210 | 12210 | yes |
| edge_tracking | OK | 396 | 396 | yes |
| harris_non_max | OK | 1002 | 1002 | yes |
| canny_non_max | OK | 2190 | 2190 | yes |
| convolution | OK | 2004 | 2004 | yes |
| elem_matrix | OK | 1812 | 1812 | yes |
| grayscale | OK | 426 | 426 | yes |
| isp | OK | 354 | 354 | yes |
| mobilenetv2 | OK* | 121411 | 121411 | yes |

\*Mobilenetv2 completed in `m5out_mobilenetv2/` (no 900s timeout). The sweep
`summary.csv` row may still show `timeout_900s` from an earlier attempt; use
`m5out_mobilenetv2/summary.csv` for the final run.

---

## Mobilenetv2 long run

| Phase | Status | Power rows | Thermal rows | Floorplan |
|-------|--------|------------|--------------|-----------|
| power_only | OK | 121,410 | 0 | no |
| power_thermal | OK | ~121,411 | ~121,411 | yes |

Runner: `util/SALAM-tools/run_mobilenetv2_traces.sh` (no timeout).

---

## Reproduce

```bash
export M5_PATH=/path/to/gem5
export ACC_BENCH_PATH=/path/to/benchmarks

util/SALAM-tools/sweep_pm_thermal_traces.sh

# Mobilenetv2 only (long)
util/SALAM-tools/run_mobilenetv2_traces.sh
```

Validate traces:

```bash
python3 configs/example/gem5_library/salam/compare_salam_power_trace.py \
  --power-trace m5out_pm_thermal_sweep/bfs/power_trace.csv

python3 configs/example/gem5_library/salam/check_salam_floorplan.py \
  --m5out m5out_pm_thermal_sweep/bfs/
```

---

## Relation to Table 2 validation

The sweep exercises **interval sampling** and **thermal feedback**, not the
paper Table 2 finalize metrics. PPA error percentages for `sys_validation`
kernels are in [TABLE2_RESULTS.md](TABLE2_RESULTS.md).
