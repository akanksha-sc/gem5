# SALAM Table 2 Validation Checkpoint

**Date:** 2026-06-10
**Branch:** `gem5-salam-develop-clocked-working`
**Reference:** Spencer et al., *Journal of Systems Architecture* 154 (2024) 103211, Table 2 (gem5-SALAMv2 column, 40 nm, 10 ns profile)

This document checkpoints SALAM stdlib Table 2 validation work: what was fixed, measured results (sim vs paper), and remaining open items.

---

## 1. What this checkpoint covers

### 1.1 Completed fixes (code in this commit)

| Area | Fix | Key files |
|------|-----|-----------|
| **Accounting / hang** | `cycle = 0` init; `hasCurrentInvocationData()` no longer gated on `cycle > 0`; `resetCurrentInvocationCounters()` after `printResults()` in `finalize()` | `llvm_interface.{cc,hh}` |
| **Call metrics** | Increment `dynComputeLaunchAttempts` + `dynComputeLaunched` when Call is enqueued; avoid double-count on return path | `llvm_interface.cc` |
| **MD-KNN hang** | Deferred `cu->initialize()` via `InitEvent`/`scheduleInitialize()`; clear `values`/`functions` before re-parse; skip declare-only functions in loop analysis; guard Call on empty callee `bbList` | `acc_compute_unit.{cc,hh}`, `comm_interface.cc`, `llvm_interface.cc` |
| **`llvm.fmuladd.*`** | `FMulAdd` instruction class + macro + `createInstruction()` detection | `LLVMRead/instruction.{cc,hh}`, `macros.hh` |
| **Weighted cycles** | `recordCommittedLatency()` on commits; `weighted_cycles` in `SALAM_SUMMARY`; used for md_knn Table 2 comparison | `llvm_interface.cc`, `compare_table2.py` |
| **Static FU area** | `computeStaticFUCounts()` counts only **top kernel** (`isTop()`) BB instructions | `llvm_interface.cc` |
| **Power model** | Restored `PowerAccumulator`, per-tick FU counting, per-cycle average dynamic (`fu_dynamic = fu_dynamic_energy / cycle`) | `salam_power_model.{cc,hh}`, `llvm_interface.cc` |
| **FU profiles** | Restored 10 ns YAMLs; `FunctionalUnits.py` updates; generator `--latency` / `--fu-only` | `FunctionalUnits.py`, `HWProfileGenerator.py`, `SALAMArgs.py` |
| **NW TopName** | Configurator `TopName` → `llvm_interface.top_name` | `config_parser.py` |
| **HWAccConfig guard** | `hasattr` guard for unknown ops (e.g. `fmuladd`) | `HWAccConfig.py` |
| **Tooling** | `compare_table2.py`, `regenerate_fu_profile.sh`, `verify_fu_profiles.py` | `util/SALAM-tools/` |

### 1.2 Validation harness status

| Benchmark | `test_metrics` | Run directory | Notes |
|-----------|----------------|---------------|-------|
| **nw** | **53/53 PASS** | `BM_ARM_OUT/fix_validation/nw_quick/` | Full `fix_validation/nw` trace lacks `SALAM_SUMMARY` (run did not finalize; window-only output) |
| **md_knn** | **53/53 PASS** | `BM_ARM_OUT/fix_validation/md_knn/` | Post-fix accounting + fmuladd |
| **bfs/fft/gemm/stencil\*** | Pre-accounting batch | `BM_ARM_OUT/stdlib_paper/` | Cycles/power mostly valid; SPM block present in older build output |

---

## 2. Metric definitions

### 2.1 Table 2 reference (paper)

- **Cycles:** kernel runtime in lockstep ticks (`runtime_cycles`), except **md_knn** uses **weighted_cycles** (latency-weighted commits).
- **Power:** paper column labeled accelerator power in mW. gem5-SALAM BFS validation established **core** = FU leakage + FU dynamic + register power, averaged over `runtime_cycles`. **Excludes SPM/CACTI** for the validated BFS row.
- **Area:** FU area (µm²) from static CDFG + YAML.

### 2.2 Simulated metrics in traces

| Trace line | Meaning |
|------------|---------|
| `Accelerator Power (core)` | FU leak + FU dyn + reg leak + reg dyn (current tree) |
| `FU Leakage` / `FU Dynamic` | Static vs dynamic datapath breakdown |
| `Register Leakage` / `Register Dynamic` | Register file breakdown (often ~0 in current runs) |
| `Accelerator Power (SPM-inclusive)` | Core + CACTI SPM leak + read/write dynamic (**only in older `stdlib_paper` traces**; not printed by current `printPowerResults()`) |
| `FU Area` / `Total Area` | Static area from power model |

**Important:** Comparing **core-only** sim power to paper Table 2 for memory-heavy kernels (fft, gemm, stencil) is misleading. SPM dynamic dominates those workloads. See §4.3.

---

## 3. Table 2 comparison — performance (cycles)

| Bench | Sim cycles | Paper cycles | Δ% | Source | Status |
|-------|-----------|--------------|-----|--------|--------|
| **bfs** | 15,600 | 15,600 | **0.00%** | `stdlib_paper/bfs` | ✅ Match |
| **fft** | 86,035 | 91,265 | **−5.73%** | `stdlib_paper/fft` | ⚠️ Close; re-run post-fix batch |
| **gemm** | 131,462 | 131,900 | **−0.33%** | `stdlib_paper/gemm` | ✅ Match |
| **md_knn** | 282,880 (weighted) | 328,025 | **−13.76%** | `fix_validation/md_knn` | ⚠️ Weighted metric; −14% gap |
| **md_knn** | 65,590 (runtime) | 328,025 | −80.00% | `fix_validation/md_knn` | ❌ Wrong metric for Table 2 |
| **nw** | 66,963 | 66,962 | **+0.00%** | `fix_validation/nw_quick` | ✅ Match |
| **stencil2d** | 109,563 | 109,563 | **0.00%** | `stdlib_paper/stencil2d` | ✅ Match |
| **stencil3d** | 47,391 | 47,210 | **+0.38%** | `stdlib_paper/stencil3d` | ✅ Match |

---

## 4. Table 2 comparison — core power (mW)

### 4.1 Summary table (core only — what `compare_table2.py` reports)

| Bench | Sim core | Paper | Δ% | FU leak | FU dyn | Reg dyn | Source |
|-------|----------|-------|-----|---------|--------|---------|--------|
| **bfs** | 1.3309 | 1.3497 | **−1.39%** | 0.1045 | 1.2264 | 0.0000 | `stdlib_paper/bfs` |
| **fft** | 0.5755 | 59.3513 | **−99.03%** | 0.0588 | 0.5167 | 0.0000 | `stdlib_paper/fft` |
| **gemm** | 2.7412 | 65.3655 | **−95.81%** | 0.4344 | 2.3068 | 0.0000 | `stdlib_paper/gemm` |
| **md_knn** | 1.8974 | 15.9594 | **−88.11%** | 0.0686 | 1.8288 | 0.0000 | `fix_validation/md_knn` |
| **nw** | 1.9826 | 6.1975 | **−68.02%** | 0.2123 | 1.7703 | 0.0000 | `fix_validation/nw_quick` |
| **stencil2d** | 1.2176 | 43.4334 | **−97.20%** | 0.1045 | 1.1131 | 0.0000 | `stdlib_paper/stencil2d` |
| **stencil3d** | 2.0224 | — | — | 0.5389 | 1.4835 | 0.0000 | `stdlib_paper/stencil3d` |

**Validated against paper (core):** BFS only (−1.4%). All other core-power gaps need SPM context or further model work.

### 4.2 SPM-inclusive power (older traces only)

Available in `BM_ARM_OUT/stdlib_paper/*/debug-trace.txt` from builds that still printed CACTI SPM. **Not** in current `printPowerResults()`.

| Bench | Core | SPM leak | SPM rd dyn | SPM wr dyn | SPM-inclusive | Paper | Δ% (SPM-inc) |
|-------|------|----------|------------|------------|---------------|-------|--------------|
| **bfs** | 1.33 | 1.80 | 17.55 | 2.33 | **23.00** | 1.35 | +1603% (SPM not in paper row) |
| **fft** | 0.58 | 1.80 | 39.36 | 19.37 | **61.11** | 59.35 | **+2.96%** |
| **gemm** | 2.74 | 1.80 | 102.44 | 8.43 | **115.41** | 65.37 | **+76.5%** |
| **stencil2d** | 1.22 | 1.80 | 53.87 | 10.23 | **67.12** | 43.43 | **+54.5%** |
| **stencil3d** | 2.02 | — | — | — | **69.73** | — | — |

### 4.3 Power interpretation

1. **BFS:** Core model validated (−1.4%). SPM-inclusive (~23 mW) is **not** the Table 2 metric.
2. **FFT:** Core-only comparison is a **metric mismatch**. SPM-inclusive (~61 mW) is within ~3% of paper. Dominant terms: SPM read dynamic (~39 mW) + write dynamic (~19 mW).
3. **GEMM / stencil:** SPM-inclusive **overshoots** paper (CACTI uses cumulative `memory_loads * 8` as UCA sizing input; heavy DMA inflates dynamic). Needs CACTI integration review.
4. **NW / md_knn:** Core power low vs paper; likely same SPM + area-accounting story. Post-fix traces lack SPM block.

### 4.4 CACTI / gem5-SALAM alignment (2026-06-16 sweep)

**Root cause (post_cacti regression):** `CommInterface::getPmemRange()` and `getReadPorts()` returned the real SPM address-range size and port count. In **gem5-SALAM**, those virtuals are **unimplemented stubs returning 0**; `LLVMInterface` then keeps defaults (`spm_size` → 4096 B for CACTI leakage, `read_ports`/`write_ports` = 2). Using the full SPM range inflated CACTI leakage and dynamic power (e.g. fft SPM-inclusive **325 mW** vs paper **59 mW**).

**Fix applied:** restore gem5-SALAM stub semantics (return 0); add `memory_loads` / `memory_stores` counters incremented only in `readCommit` / `writeCommit` (not internal/register loads); feed those into `computeSpmPower()`.

**Batch:** `BM_ARM_OUT/post_cacti_fix/` (`--paper-timing --test-metrics`, all 53/53 pass).

| Bench | SPM-inclusive (fix) | SPM-inclusive (broken post_cacti) | stdlib_paper | Paper | Notes |
|-------|---------------------|-----------------------------------|--------------|-------|-------|
| **fft** | **116.3** | 325.5 | 61.1 | 59.4 | Fix recovers ~2.8×; still ~2× above Jun-9 stdlib (full CACTI link) |
| **gemm** | **266.1** | 1540 | 115.4 | 65.4 | Same pattern |
| **stencil2d** | **145.4** | 159.2 | 67.1 | 43.4 | |
| **nw** | **130.4** | 399.4 | — | 6.2 | SPM block now present |
| **md_knn** | **43.1** | 170.8 | — | 16.0 | |

**gem5-SALAM power model (confirmed):** Table 2 memory-heavy rows match **core (FU+reg) + SPM CACTI** (`Accelerator Power (SPM)`), not a separate “local bus only” path. `memory_loads` counts all accelerator load commits (debug label: “Local Read Commit”), including SPM traffic. A separate **cache** CACTI path exists in gem5-SALAM but is not what matched fft paper.

**Do not revert** the CACTI SConscript link or `computeSpmPower()` — only the `getPmemRange`/`getReadPorts` override was wrong. Remaining gap vs `stdlib_paper` (~2× SPM dynamic) correlates with **fully linked** `ext/mcpat/cacti` (Jun-12+ build) vs older traces; investigate CACTI object parity with gem5-SALAM next (P8).

**Cycles regression (bfs):** **Fixed (2026-06-16):** `RegisterBank` now sends **0-latency reads immediately** (gem5-SALAM semantics); `runtime_cycles` restored to **15,600** under `--paper-timing`. Batch: `BM_ARM_OUT/post_regfix/`.

---

## 5. Table 2 comparison — FU area (µm²)

| Bench | Sim FU area | Paper | Δ% | Source | Notes |
|-------|-------------|-------|-----|--------|-------|
| **bfs** | 4,941.5 | 7,696 | **−35.8%** | `stdlib_paper/bfs` | Under-count vs paper |
| **fft** | 7,267.6 | 39,228 | **−81.5%** | `stdlib_paper/fft` | Large gap; paper trace used wrong ref label (7696) |
| **gemm** | 121,558.6 | 289,400 | **−58.0%** | `stdlib_paper/gemm` | Under-count but still large |
| **md_knn** | 38,836.2 | 42,791 | **−9.2%** | `fix_validation/md_knn` | ✅ Close |
| **nw** | 58,356.1 | 12,152 | **+380.2%** | `fix_validation/nw_quick` | ❌ `pad_tail` helper inflates static FU; top-only filter insufficient |
| **stencil2d** | 5,775.2 | 9,000 | **−35.8%** | `stdlib_paper/stencil2d` | Under-count |
| **stencil3d** | 98,443.3 | — | — | `stdlib_paper/stencil3d` | No paper area |

---

## 6. Reproduce

```bash
# Build (example)
scons build/ARM/gem5.opt -j$(nproc)

# Single benchmark with paper timing
util/SALAM-tools/run_system_stdlib.sh --bench nw --paper-timing \
  --outdir BM_ARM_OUT/fix_validation/nw_quick

# Compare against Table 2
python3 util/SALAM-tools/compare_table2.py BM_ARM_OUT/fix_validation/nw_quick

# Metric tests (53 checks)
util/SALAM-tools/validate_stdlib.sh BM_ARM_OUT/fix_validation/nw_quick
```

Batch reference runs live under `BM_ARM_OUT/stdlib_paper/` (pre-accounting-fix batch, still useful for SPM-inclusive numbers).

---

## 7. Known TODOs (remaining)

### 7.1 High priority

| ID | Item | Notes |
|----|------|-------|
| P1 | **Re-port CACTI SPM block** into `printPowerResults()` | **Done (2026-06-12):** CACTI linked in `SConscript`; SPM leak/rd/wr + SPM-inclusive printed |
| P2 | **Update `compare_table2.py`** | **Done (2026-06-12):** Core + SPM-inclusive tables + FU/SPM breakdown |
| P3 | **Post-fix `--paper-timing` batch** | **Done (2026-06-16):** `BM_ARM_OUT/post_regfix/` (7/7 benches, 53/53 metrics) |
| P9 | **RegisterBank 0-cycle reads** | **Done (2026-06-16):** immediate `sendTimingResp` when `read_latency_cycles==0`; writes keep tick-engine visibility |
| P8 | **CACTI dynamic 2× vs stdlib_paper** | Full `cacti_interface` link vs Jun-9 traces; align `ext/mcpat/cacti` build with gem5-SALAM |
| P4 | **NW area inflation** | IR includes `pad_tail` helper not in gem5-SALAM `nw.ll`; top-only static FU count still +380% vs paper |
| P5 | **GEMM/stencil SPM overshoot** | Review CACTI `memory_loads * 8` sizing; per-access vs cumulative traffic |
| P6 | **md_knn weighted_cycles −14%** | Investigate latency accounting vs paper HLS pipeline model |
| P7 | **NW/md_knn core power vs paper** | Determine if SPM re-port closes gap or separate FU/static issue |

### 7.2 Medium priority

| ID | Item |
|----|------|
| M1 | Fix `fix_validation/nw` run finalization | **Mitigated:** `window_stats_enable` default now `False` (was flooding trace) |
| M2 | BFS FU area −36% vs paper (7.7k µm²) |
| M3 | FFT FU area −81% vs paper (39k µm²) |
| M4 | Regenerate all 10 ns FU YAMLs after any `FunctionalUnits.py` change |
| M5 | External benchmark repo: ensure `md_knn` fmuladd in `config.yml` + IR |

### 7.3 Unknown / needs investigation

| ID | Question |
|----|----------|
| U1 | Does paper Table 2 power for fft/gemm/stencil include memory energy in practice, despite BFS docs saying "core only"? |
| U2 | Correct CACTI invocation: size parameter vs access count vs SPM configured capacity |
| U3 | Should FU dynamic use `useful_compute` cycles instead of `runtime_cycles` for memory-bound kernels? |
| U4 | NW `agg_cycles` after fix: verify rollup across invocations in production runs |
| U5 | gemm paper FU area 289k µm² vs sim 122k — unroll/pipeline static instance mismatch? |

---

## 8. Files changed in this commit (summary)

- **Simulator core:** `llvm_interface.{cc,hh}`, `acc_compute_unit.{cc,hh}`, `comm_interface.cc`, `salam_power_model.{cc,hh}`, `LLVMRead/*`, `FunctionalUnits.py`
- **Configs:** `HWAcc.py`, `HWAccConfig.py`, `salam_arm_board.py`, `run_salam_stdlib.py`, bfs hw defines
- **Tools:** `config_parser.py`, `HWProfileGenerator.py`, `SALAMArgs.py`, `run_system_stdlib.sh`, `validate_stdlib.sh`
- **New:** `compare_table2.py`, `regenerate_fu_profile.sh`, `verify_fu_profiles.py`, this document

**Not committed:** `BM_ARM_OUT/` (simulation artifacts), `ext/mcpat/cacti/*.o`, test logs, temp files.

---

## 9. Quick status matrix

| Bench | Cycles | Core power | SPM-inclusive | FU area | test_metrics |
|-------|--------|------------|---------------|---------|--------------|
| bfs | ✅ | ✅ | N/A (not paper metric) | ⚠️ | — |
| fft | ⚠️ | ❌ core / ✅ SPM-inc | ✅ ~3% | ❌ | — |
| gemm | ✅ | ❌ | ❌ overshoot | ⚠️ | — |
| md_knn | ⚠️ weighted | ❌ | TBD | ✅ | ✅ 53/53 |
| nw | ✅ | ❌ | TBD | ❌ | ✅ 53/53 |
| stencil2d | ✅ | ❌ core | ⚠️ +55% | ⚠️ | — |
| stencil3d | ✅ | — | — | — | — |

Legend: ✅ within ~5% or validated; ⚠️ partial; ❌ significant gap; — not measured / not in paper.
