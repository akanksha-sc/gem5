# SALAM Power Model Validation Status

**Branch:** `gem5-salam-develop-clocked-working`
**Last updated:** 2026-07-05
**Related:** [Table 2 validation](../SALAM-tools/docs/TABLE2_VALIDATION.md), [power/thermal sampling plan](../../configs/example/gem5_library/salam_pm/)

This document tracks which memory and compute power models are validated, how they are modeled, and what remains for interval sampling and thermal feedback.

---

## 1. Two power stacks in this checkout

| Stack | Scope | Model source | Validation phase |
|-------|--------|--------------|------------------|
| **gem5-pm (host)** | ARM CPU, L1I, L1D, L2 | McPAT + HotSpot regression | Phase 2 (`arm_a9_with_mcpat_and_hotspot.py`) |
| **SALAM PM (accelerator)** | FU datapath, register file, SPM | FU library + `_bit_register` + CACTI | Phase 3 stats; Phase 4 interval sampling |

Host DRAM and system memory are **not** modeled by SALAM PM. Accelerator-local structures (SPM, register file, optional private acc cache in upstream gem5-SALAM) are **not** covered by McPAT L1/L2 configs.

---

## 2. Component validation matrix

| Component | Where modeled | Technique | Validated? | Notes |
|-----------|---------------|-----------|------------|-------|
| **FU datapath** | `SALAMPowerModel` | Per-tick FU counts × 10 ns profile energies | **Yes** | Table 2 core power; Phase 3–4 stat deltas match `printPowerResults` (~0.1% on BFS) |
| **Register file** | `SALAMPowerModel` | `_bit_register` FU, incremental read/write | **Yes** | Leakage topped up at finalize; dynamic per access |
| **SPM** | `SALAMPowerModel` + `cacti_wrapper` | CACTI UCA at `spm_size` | **Partial** | Table 2 fft/gemm historically OK at finalize; interval sampling required **per-access** dynamic (see §3) |
| **Stream buffers** | — | None | **No** | Not instrumented in mainline gem5-SALAM port |
| **Private acc cache** | gem5-SALAM only | CACTI | **Not ported** | Exists in upstream gem5-SALAM; not in this tree |
| **Host L1I / L1D / L2** | gem5-pm | McPAT | **Yes** | Phase 2 harness vs reference traces |
| **Global / host DRAM** | gem5 (optional) | Not wired for SALAM stdlib runs | **N/A** | Out of SALAM PM scope |

---

## 3. SPM CACTI modeling (current vs target)

### 3.1 Finalize-only technique (pre per-access fix)

At simulation end, `LLVMInterface::printPowerResults()` called `computeSpmPower()` with:

- **Leakage:** CACTI array sized to real `spm_size` (bytes).
- **Read/write dynamic:** CACTI arrays sized to `memory_loads × 8` and `memory_stores × 8` bytes — a nonstandard scaling tied to access count, not per-access energy at fixed capacity.

Totals were written once via `syncFinalizedComponentStats()`, so **interval stat deltas for SPM were zero until finalize** (one spike in `power_trace.csv`).

### 3.2 Per-access technique (Phase 4 fix)

| Quantity | When updated | Formula |
|----------|--------------|---------|
| Read dynamic energy | Each SPM read (`noteSpmRead`) | `spm_per_access_read_mw` × 1 cycle |
| Write dynamic energy | Each SPM write (`noteSpmWrite`) | `spm_per_access_write_mw` × 1 cycle |
| SPM leakage | Each accelerator cycle (`updateCycle`) | `spm_leakage_mw` × 1 cycle |

Per-access mW values come from `computeSpmPerAccess(spm_bytes, read_ports, write_ports)` — CACTI read/write ops on an array sized to **full `spm_size`**, independent of access count.

`printPowerResults()` reports averages as `(per_access × count) / cycles` for dynamic and `spm_leakage_mw` for static.

---

## 4. Interval power and thermal sampling status

Implementation lives under `configs/example/gem5_library/salam_pm/`.

| Phase | Goal | Status |
|-------|------|--------|
| **1** | Port `PowerModelPyFunc`, `ThermalModel`, interval stats API | Done (`b61d37f785`) |
| **2** | CPU + L1/L2 McPAT validation harness | Done (`e2fa37dc23`) |
| **3** | SALAM per-component gem5 stats (`power.componentEnergy`, `power.accCycles`) | Done (`d610b978ab`) |
| **4** | SALAM `SalamBlockPowerModel`, per-block thermal domains, leakage regression, power trace | **Done** — per-access SPM CACTI, power-state ON, interval trace validated on BFS (~0.1–0.8% vs stats) |
| **5** | HotSpot floorplan for SALAM blocks, ROI hypercalls, closed-loop thermal on bfs/gemm/md_knn | **Done** — HotSpot per-block floorplan, hypercall 1999/2000 ROI, closed-loop validated on BFS |

### Phase 5 CLI (HotSpot + ROI)

```bash
build/ARM/gem5.opt configs/example/gem5_library/salam/run_salam_stdlib.py \
  --bench bfs ... \
  --salam-power-sampling \
  --salam-thermal-sampling \
  --salam-roi-sampling \
  --salam-thermal-solver hotspot \
  --power-interval-cycles 1000 \
  --thermal-interval-cycles 5000 \
  --power-trace-debug --thermal-trace-debug
```

Workloads must call `m5_hypercall(SALAM_ROI_BEGIN)` / `m5_hypercall(SALAM_ROI_END)` around the accelerator kernel (see `benchmarks/common/m5ops.h`). Use `--salam-power-auto-start` for bring-up without ROI markers.

### Phase 4 CLI (stdlib runner)

```bash
build/ARM/gem5.opt configs/example/gem5_library/salam/run_salam_stdlib.py \
  --bench bfs ... \
  --salam-power-sampling \
  --salam-power-auto-start \
  --power-interval-ticks 1000 \
  --power-trace-debug
```

Optional thermal (simple RC solver for smoke tests):

```bash
  --salam-thermal-sampling \
  --salam-thermal-solver simple \
  --thermal-interval-cycles 5000
```

### Validation scripts

| Script | Purpose |
|--------|---------|
| `salam/check_salam_pm_stats.py` | Window sums of `power.componentEnergy` vs `printPowerResults` |
| `salam/compare_salam_power_trace.py` | Active-interval dynamic power in trace vs stat deltas |
| `salam/compare_salam_thermal_loop.py` | Closed-loop thermal: power intervals + temperature rise |
| `salam/check_salam_pm_domains.py` | Domain wiring smoke test |

---

## 6. Known gaps and follow-ups

1. **Stream buffers** — no power model; add if SPM traffic modeling requires it.
2. **Private accelerator cache** — port CACTI path from gem5-SALAM if `--acc-cache` is used in production configs.
3. **Register leakage during run** — still finalized in one shot; only SPM leakage is per-cycle today. Acceptable for interval sampling if reg leakage ≪ FU/SPM.
4. **`CommInterface::getPmemRange()`** — still returns 0 (gem5-SALAM stub); C++ PM uses 4096-byte default until real SPM sizing is wired.
5. **Last partial interval** — `sample_power_now()` before `stop_power_sampling()` at end of run (implemented in stdlib runner).
6. **`llvm_interface` power state** — `llvm.power_state.default_state = "ON"` when attaching SALAM PMs (in `collect_salam_power_bindings()`).
7. **Temperature feedback** — leakage(T) regression in Python PM; HotSpot floorplan provides spatial thermal coupling in Phase 5.

---

## 7. References

- Spencer et al., *J. Systems Architecture* 154 (2024) 103211 — Table 2 paper targets.
- `util/SALAM-tools/docs/TABLE2_VALIDATION.md` — cycle/core-power comparison checkpoint.
- `configs/example/gem5_library/salam_pm/static_regression.py` — leakage vs temperature for SALAM blocks.
- `src/salam/HWModeling/salam_power_model.{hh,cc}` — C++ accumulator and gem5 stats.
- `src/salam/HWModeling/cacti_wrapper.{hh,cc}` — CACTI SPM helpers.
