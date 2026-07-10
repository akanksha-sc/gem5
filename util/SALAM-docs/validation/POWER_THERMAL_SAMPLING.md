# SALAM Interval Power and Thermal Sampling

**Branch:** `gem5-salam-develop-clocked-working`
**Implementation:** `configs/example/gem5_library/salam_pm/`

Table 2 **finalize-only** PPA validation is documented in
[TABLE2_RESULTS.md](TABLE2_RESULTS.md). This document covers **runtime**
power/thermal sampling added for closed-loop modeling.

---

## Two power stacks

| Stack | Scope | Model | Validation |
|-------|-------|-------|------------|
| **gem5-pm (host)** | ARM CPU, L1I, L1D, L2 | McPAT + HotSpot | Phase 2 harness |
| **SALAM PM (accelerator)** | FU, register file, SPM | FU library + CACTI | Phases 3–5b |

Host DRAM is outside SALAM PM scope.

---

## Component status

| Component | Modeled | Table 2 validated | Interval sampling |
|-----------|---------|-------------------|-------------------|
| FU datapath | ✅ | ✅ core power | ✅ stat deltas ~0.1% vs finalize |
| Register file | ✅ | ✅ | ✅ |
| SPM (CACTI) | ✅ | ✅ (finalize) | ✅ per-access dynamic (Phase 4) |
| Stream buffers | ❌ | — | — |
| Private acc cache | ❌ (not ported) | — | — |
| HotSpot floorplan | ✅ Phase 5b | — | Area from `power.{fu,reg,spm}AreaUm2` |

---

## Phase completion

| Phase | Goal | Status |
|-------|------|--------|
| 1 | Port `PowerModelPyFunc`, `ThermalModel`, interval stats | Done |
| 2 | CPU + L1/L2 McPAT validation | Done |
| 3 | SALAM per-component gem5 stats | Done |
| 4 | `SalamBlockPowerModel`, per-access SPM, power trace | Done |
| 5 | HotSpot floorplan, ROI hypercalls, closed-loop thermal | Done |
| 5b | Area-proportional floorplan from C++ block areas | Done |

---

## Example CLI

Power + thermal + ROI + area floorplan:

```bash
build/ARM/gem5.opt configs/example/gem5_library/salam/run_salam_stdlib.py \
  --bench bfs \
  --salam-power-sampling \
  --salam-thermal-sampling \
  --salam-roi-sampling \
  --salam-thermal-solver hotspot \
  --salam-floorplan-geometry area \
  --power-interval-cycles 1000 \
  --thermal-interval-cycles 5000
```

Power-only smoke:

```bash
  --salam-power-sampling \
  --salam-power-auto-start \
  --power-interval-ticks 1000
```

---

## Validation scripts

| Script | Purpose |
|--------|---------|
| `salam/check_salam_pm_stats.py` | Window sums vs `printPowerResults` |
| `salam/compare_salam_power_trace.py` | Trace dynamic power vs stat deltas |
| `salam/compare_salam_thermal_loop.py` | Closed-loop thermal rise |
| `salam/check_salam_floorplan.py` | Area stats + floorplan JSON |

---

## SPM modeling notes

**Finalize (Table 2):** averages over full kernel runtime.
**Interval (Phase 4):** per-access read/write mW from CACTI at full `spm_size`;
leakage accrues each accelerator cycle.

`CommInterface::getPmemRange()` still returns 0 (gem5-SALAM stub); C++ PM uses
4096-byte default until real SPM sizing is wired.

---

## Known gaps

1. Stream buffers — not instrumented.
2. Register leakage — finalized in one shot (acceptable if ≪ FU/SPM).
3. Last partial power interval — sampled before `stop_power_sampling()` at EoS.
4. SPM-inclusive interval totals may differ from historical finalize traces (~2× on some kernels); separate from Table 2 core-power validation.
