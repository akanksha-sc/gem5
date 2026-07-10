# SALAM Power Model Validation Status

**Branch:** `gem5-salam-develop-clocked-working`
**Last updated:** 2026-07-07

> **Canonical validation docs:** [util/SALAM-docs/validation/](validation/)

| Document | Contents |
|----------|----------|
| [validation/TABLE2_RESULTS.md](validation/TABLE2_RESULTS.md) | **PPA error % vs paper Table 2** (primary) |
| [validation/IMPLEMENTATION.md](validation/IMPLEMENTATION.md) | Fixes, metrics, calibration |
| [validation/POWER_THERMAL_SAMPLING.md](validation/POWER_THERMAL_SAMPLING.md) | Interval PM/thermal Phases 1–5b |
| [validation/PM_THERMAL_SWEEP.md](validation/PM_THERMAL_SWEEP.md) | Full benchmark sweep results |

---

## Quick summary

Table 2 **sys_validation** benchmarks (post power/thermal integration) match
paper **cycles** and **core power** within **~0–8%** when traces are parsed
with `compare_table2.py` / `validation_salam.py`. Four benchmarks are at **0%**
cycle error (bfs, md_knn, nw, stencil2d). The old
`validation_results/VALIDATION_RESULTS.md` (July 4) showed 39–102% cycle errors
due to comparing **weighted_cycles** instead of kernel `runtime_cycles`.

Regenerate reports:

```bash
python3 util/SALAM-tools/generate_validation_report.py --m5-path .
```

---

## Two power stacks (unchanged)

| Stack | Scope | Validation phase |
|-------|--------|------------------|
| **gem5-pm (host)** | ARM CPU, L1/L2 | Phase 2 |
| **SALAM PM (accelerator)** | FU, regfile, SPM | Phases 3–5b + Table 2 finalize |

See [validation/POWER_THERMAL_SAMPLING.md](validation/POWER_THERMAL_SAMPLING.md) for phase details and CLI examples.
