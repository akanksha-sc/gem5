# SALAM Validation Documentation

This folder is the canonical home for SALAM power, performance, and area (PPA)
validation against Spencer et al. JSA 2024 Table 2 and extended `sys_validation`
benchmarks.

## Documents

| Document | Purpose |
|----------|---------|
| [TABLE2_RESULTS.md](TABLE2_RESULTS.md) | **Final PPA error % table** (auto-generated; primary deliverable) |
| [IMPLEMENTATION.md](IMPLEMENTATION.md) | Simulator fixes, metric definitions, calibration knobs |
| [PROGRESS.md](PROGRESS.md) | Timeline of validation milestones and open items |
| [POWER_THERMAL_SAMPLING.md](POWER_THERMAL_SAMPLING.md) | Interval power/thermal sampling (Phases 1–5b) |
| [PM_THERMAL_SWEEP.md](PM_THERMAL_SWEEP.md) | Full benchmark sweep with power + thermal traces |

## Quick commands

```bash
# Regenerate TABLE2_RESULTS.md from existing traces
python3 util/SALAM-tools/generate_validation_report.py --m5-path .

# Re-run Table 2 benchmarks (bfs … stencil2d)
python3 util/SALAM-tools/validation_salam.py \
  --m5-path . \
  --acc-bench-path /path/to/benchmarks/sys_validation

# Compare one trace directory
python3 util/SALAM-tools/compare_table2.py validation_results/bfs
```

## Trace locations

| Suite | Directory | Notes |
|-------|-----------|-------|
| Table 2 (6 benches) | `validation_results/{bfs,fft,gemm,md_knn,nw,stencil2d}/` | `validation_salam.py` @ 100 MHz / 500 MHz SPM |
| stencil3d cycles | `BM_ARM_OUT/post_regfix/stencil3d/` | `--paper-timing`; stale trace in `validation_results/stencil3d` |
| Extended sys_validation | `validation_results/{md_grid,mergesort,spmv}/` | No paper reference |
| PM/thermal sweep | `m5out_pm_thermal_sweep/` | Power + thermal interval sampling |
| Mobilenetv2 | `m5out_mobilenetv2/` | Long-running CNN (no Table 2 ref) |

## Tools (in `util/SALAM-tools/`)

- `validation_salam.py` — run + parse Table 2 benchmarks
- `compare_table2.py` — parse `debug-trace.txt` vs paper
- `generate_validation_report.py` — write this folder's `TABLE2_RESULTS.md`

Legacy checkpoint notes previously in `util/SALAM-tools/docs/TABLE2_VALIDATION.md`
are superseded by the documents here.
