#!/usr/bin/env bash
set -euo pipefail

# Resolve repo root from:
# configs/example/gem5_library/eval/run_integrated_large.sh
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../../.." && pwd)"

GEM5_BIN="${REPO_ROOT}/build/ARM/gem5.opt"
CFG="${REPO_ROOT}/configs/example/gem5_library/arm_a9_with_mcpat_and_hotspot.py"
OUT_ROOT="${REPO_ROOT}/m5out_eval_large"

POWER_THERMAL_INTERVAL_CYCLES="${POWER_THERMAL_INTERVAL_CYCLES:-500000}"
# At 2 GHz (500 ps/cycle), convert cycles to gem5 ticks (1 ps/tick).
CLK_PERIOD_PS=500
POWER_THERMAL_INTERVAL_TICKS=$((POWER_THERMAL_INTERVAL_CYCLES * CLK_PERIOD_PS))

if [[ ! -x "${GEM5_BIN}" ]]; then
    echo "ERROR: gem5 binary not found or not executable: ${GEM5_BIN}"
    exit 1
fi

if [[ ! -f "${CFG}" ]]; then
    echo "ERROR: config script not found: ${CFG}"
    exit 1
fi

mkdir -p "${OUT_ROOT}"

# app -> (N, REPEATS)
run_app () {
    local app="$1"
    local n="$2"
    local repeats="$3"
    local outdir="${OUT_ROOT}/${app}"

    echo "============================================================"
    echo "Running integrated power+thermal evaluation for ${app}"
    echo "  output dir       : ${outdir}"
    echo "  workload args    : ${n} ${repeats}"
    echo "  power interval   : ${POWER_THERMAL_INTERVAL_CYCLES} cycles (${POWER_THERMAL_INTERVAL_TICKS} ticks)"
    echo "  thermal interval : ${POWER_THERMAL_INTERVAL_CYCLES} cycles (${POWER_THERMAL_INTERVAL_TICKS} ticks)"
    echo "============================================================"

    "${GEM5_BIN}" -d "${outdir}" \
        "${CFG}" \
        --cpu_type timing \
        --workload "${app}" \
        --workload-args "${n}" "${repeats}" \
        --power_interval_ticks "${POWER_THERMAL_INTERVAL_TICKS}" \
        --thermal_interval_ticks "${POWER_THERMAL_INTERVAL_TICKS}" \
        --power_trace_debug \
        --thermal_trace_debug \
        --hotspot_ambient_temp_k 300.0 \
        --hotspot_initial_temp_k 300.0
}

run_app saxpy 262144 1000
run_app iaxpy 262144 1000
run_app daxpy 131072 1000

echo
echo "Integrated runs complete."
echo "Outputs are in: ${OUT_ROOT}"
echo "Each app directory should contain power_trace.csv and thermal_trace.csv"
