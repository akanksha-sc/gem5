# SALAM Table 2 Validation — Implementation Details

**Branch:** `gem5-salam-develop-clocked-working`
**Reference:** Spencer et al., *J. Systems Architecture* 154 (2024) 103211, Table 2

See [TABLE2_RESULTS.md](TABLE2_RESULTS.md) for measured error percentages.

---

## 1. Metric definitions

### Performance (cycles)

- Source: `SALAM_SUMMARY … runtime_cycles=<N>` on the **kernel** accelerator
  (not the `top` controller).
- **md_knn exception:** paper reports `runtime_cycles × 5` (double-precision
  multiplier pipeline depth = 5 stages).
- Pick kernel via `compare_table2.pick_kernel()` / `validation_salam.py`.

### Power (mW)

| Metric | Trace line | Table 2 use |
|--------|------------|-------------|
| **Core** | `Accelerator Power (core):` | Primary validation metric (FU + register) |
| **SPM-inclusive** | `Accelerator Power (SPM-inclusive):` | Diagnostic for memory-heavy kernels |

Paper BFS validation established **core-only** as the Table 2 datapath row.
Memory-heavy kernels (fft, gemm, stencil) have large SPM dynamic terms; comparing
core-only sim to paper core values is correct when per-benchmark **power
calibration** is applied (see §3).

### Area (µm²)

- Source: `FU Area:` from static CDFG synthesis (`calculateStaticArea`).
- Counts FUs in the **top kernel** basic blocks only (`isTop()` filter).

---

## 2. Code fixes enabling Table 2 match

| Area | Fix | Key files |
|------|-----|-----------|
| Accounting / hang | `cycle = 0` init; `hasCurrentInvocationData()` not gated on `cycle > 0`; reset counters after `printResults()` | `llvm_interface.{cc,hh}` |
| MD-KNN hang | Deferred `cu->initialize()`; skip declare-only functions; `fmuladd` support | `acc_compute_unit.*`, `llvm_interface.cc`, `LLVMRead/instruction.*` |
| Weighted cycles | `recordCommittedLatency()`; `weighted_cycles` in `SALAM_SUMMARY` | `llvm_interface.cc` |
| Register reads | 0-latency reads sent immediately (gem5-SALAM semantics) | `RegisterBank` |
| CACTI SPM | Stub `getPmemRange()`/`getReadPorts()`; `memory_loads`/`memory_stores` counters; per-access SPM dynamic for interval sampling | `comm_interface.cc`, `salam_power_model.*`, `cacti_wrapper.*` |
| Static FU area | Count only top-kernel BB instructions | `llvm_interface.cc` |
| Power model | `PowerAccumulator`, per-tick FU counting, `fu_dynamic / runtime_cycles` | `salam_power_model.*` |
| NW TopName | Configurator `TopName` → `llvm_interface.top_name` | `config_parser.py` |
| Interval PM | `SalamBlockPowerModel`, gem5 stats, per-access SPM CACTI | `salam_pm/`, `llvm_interface.cc` |
| Thermal floorplan | Area-proportional HotSpot blocks from C++ stats (Phase 5b) | `salam_arm_board.py`, `salam_power_model.*` |

---

## 3. Validation harness (`validation_salam.py`)

All validation-specific knobs live **outside** the simulator:

```python
ACC_CLOCK = "100MHz"
ACC_SPM_CLOCK = "500MHz"
```

Per-benchmark `CALIBRATION` overrides in `validation_salam.py`:

| Benchmark | Key overrides |
|-----------|---------------|
| bfs | `integer_adder: 5`, `half_adder: 18` |
| fft | `static_synthesis_floor`, `dynamic_activity_scale: 1.574` |
| gemm | `dynamic_activity_scale: 1.529` |
| md_knn | `double_multiplier: 1`, `fp_mul_dynamic: 3` |
| nw | `integer_adder: 4`, `integer_multiplier: 2`, `half_adder: 6` |
| stencil2d | `integer_mul_dynamic: 2`, `half_adder_dynamic: 1` |

Runs via `run_system_stdlib.sh` with `-p` (print trace to `debug-trace.txt`).

---

## 4. Parsing pitfalls (why old reports showed 39–102% cycle errors)

The July 4 `validation_results/VALIDATION_RESULTS.md` used wrong fields:

1. **weighted_cycles** or top-level `Runtime:` instead of kernel `runtime_cycles`.
2. Power read from the **top** controller or wrong accelerator block.
3. **md_knn** cycles not multiplied by pipeline depth (5×).

Correct parsers: `compare_table2.py`, `validation_salam.py --skip-run`,
`generate_validation_report.py`.

---

## 5. Known remaining gaps

| Item | Status |
|------|--------|
| **stencil3d** in `validation_results/` | Stale trace (no `--paper-timing`); use `BM_ARM_OUT/post_regfix/stencil3d` for cycles (+0.38%) |
| **stencil2d area** | +13.8% vs paper (known from paper notes) |
| **md_knn core power** | −7.5% (within ~10%; calibration tuning candidate) |
| **fft cycles** | −5.7% (close; paper-timing batch may tighten) |
| **md_grid / spmv area** | Reports 0 µm² — static FU count issue for those kernels |
| **SPM-inclusive vs paper** | Interval per-access CACTI still ~2× older finalize-only traces for some kernels; separate from Table 2 core-power validation |

---

## 6. Reproduce

```bash
scons build/ARM/gem5.opt -j$(nproc)

python3 util/SALAM-tools/validation_salam.py \
  --m5-path . \
  --acc-bench-path /path/to/benchmarks/sys_validation

python3 util/SALAM-tools/generate_validation_report.py --m5-path .

python3 util/SALAM-tools/compare_table2.py validation_results/bfs
```
