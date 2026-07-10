# SALAM Validation Progress Log

Chronological checkpoint of Table 2 and PM/thermal validation work.

---

## 2026-06-10 — Table 2 accounting fixes

- Fixed invocation accounting hang (`cycle=0`, finalize counters).
- MD-KNN: deferred init, `fmuladd`, weighted cycles.
- Added `compare_table2.py`, initial `TABLE2_VALIDATION.md` checkpoint.
- **Cycles:** bfs/nw/stencil2d at 0%; gemm −0.33%; fft −5.7%.

## 2026-06-12 — CACTI SPM re-port

- Linked CACTI in SConscript; SPM leak/rd/wr + SPM-inclusive in traces.
- `compare_table2.py`: core + SPM-inclusive tables.

## 2026-06-16 — RegisterBank + CACTI stub fix

- 0-cycle register reads restored bfs to **15,600** cycles.
- `getPmemRange()`/`getReadPorts()` stub semantics (gem5-SALAM parity).
- Batch: `BM_ARM_OUT/post_regfix/` (7/7 benches, 53/53 metric tests).

## 2026-07-04 — `validation_salam.py` batch

- Ran 6 Table 2 benchmarks → `validation_results/{bfs,fft,gemm,md_knn,nw,stencil2d}/`.
- **Core power within ~5%** on all six when parsed correctly.
- Extended benches (md_grid, mergesort, spmv, stencil3d) in same tree.
- Stale human-written `VALIDATION_RESULTS.md` used wrong cycle field (superseded).

## 2026-07-05 — Power/thermal sampling Phases 1–5b

- Interval power stats, per-access SPM CACTI, HotSpot floorplan from C++ areas.
- Documented in [POWER_THERMAL_SAMPLING.md](POWER_THERMAL_SAMPLING.md).

## 2026-07-07 — PM/thermal sweep + mobilenetv2

- `m5out_pm_thermal_sweep/`: 17/18 OK (vision + sys_validation).
- `m5out_mobilenetv2/`: full power + thermal run completed (no timeout).
- Validation docs consolidated under `util/SALAM-docs/validation/`.
- `generate_validation_report.py` produces canonical [TABLE2_RESULTS.md](TABLE2_RESULTS.md).

---

## Current Table 2 status (core power metric)

| Benchmark | Cycles | Core power | Area | Overall |
|-----------|--------|------------|------|---------|
| bfs | ✅ 0% | ✅ −0.07% | ✅ +0.38% | ✅ |
| fft | ⚠️ −5.7% | ✅ −0.37% | ✅ −1.8% | ⚠️ |
| gemm | ✅ −0.33% | ✅ −2.2% | ⚠️ −5.7% | ⚠️ |
| md_knn | ✅ 0% | ⚠️ −7.5% | ✅ −1.6% | ⚠️ |
| nw | ✅ 0% | ✅ +5.0% | ✅ +0.26% | ✅ |
| stencil2d | ✅ 0% | ✅ +2.6% | ❌ +13.8% | ❌ area |
| stencil3d | ✅ +0.38%* | — | — | cycles only |

\*from `BM_ARM_OUT/post_regfix/stencil3d` (`--paper-timing`).

**Takeaway:** Post-integration of power/thermal sampling, Table 2 **performance
and core power** match paper within ~0–8% for all reported benchmarks except
stencil2d area. High errors in the old summary were a **documentation/parsing
bug**, not a simulator regression.

---

## Open items

1. Re-run `validation_salam.py` for stencil3d with `--paper-timing` into `validation_results/stencil3d`.
2. Investigate stencil2d FU area +14% vs paper.
3. Fix md_grid/spmv zero FU area reporting.
4. Align SPM-inclusive interval sampling with historical finalize-only traces (P8).
5. Wire `generate_validation_report.py` into CI or post-run hook.
