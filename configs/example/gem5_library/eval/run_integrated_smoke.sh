#!/usr/bin/env bash
set -euo pipefail

# Quick Phase 2 smoke test: saxpy ROI with tick-based power/thermal sampling.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../../.." && pwd)"

GEM5_BIN="${REPO_ROOT}/build/ARM/gem5.opt"
CFG="${REPO_ROOT}/configs/example/gem5_library/arm_a9_with_mcpat_and_hotspot.py"
OUTDIR="${REPO_ROOT}/m5out_phase2_smoke"

# 1000 cycles @ 2 GHz = 500000 ticks
INTERVAL_TICKS=500000

"${GEM5_BIN}" -d "${OUTDIR}" "${CFG}" \
    --cpu_type timing \
    --workload saxpy \
    --workload-args 8192 50 \
    --power_interval_ticks "${INTERVAL_TICKS}" \
    --thermal_interval_ticks "${INTERVAL_TICKS}" \
    --power_trace_debug \
    --thermal_trace_debug \
    --hotspot_ambient_temp_k 300.0 \
    --hotspot_initial_temp_k 300.0

echo
echo "Smoke test complete. Check:"
echo "  ${OUTDIR}/power_trace.csv"
echo "  ${OUTDIR}/thermal_trace.csv"
