#!/usr/bin/env bash
set -euo pipefail

# Convert gem5 power_trace.csv -> HotSpot .ptrace and run native HotSpot.
#
# Default: COLD transient only (no steady / -init_file). This matches the
# integrated runtime, which steps from domain initial temperature (300 K).
#
# Warm replay (optional): set RUN_WARM=1 to also run steady + init-based
# transient and write <app>_warm.ttrace. Do NOT use warm .ttrace for
# validation against integrated thermal_trace.csv.
#
# Evaluation tier (separate from the other tier's directories):
#   EVAL_TIER=large  (default)  -> m5out_eval_large, standalone_hotspot_out, 500k config
#   EVAL_TIER=xl     -> m5out_eval_xl, standalone_hotspot_out_xl, 5M config
# Or: ./run_standalone_hotspot_xl.sh
#
# Config must match gem5 POWER_THERMAL_INTERVAL for that run:
#   500000 / 5000000 / 10000000 cycles @ 2 GHz  ->  matching arm_a9_runtime_block_*.config
# Override: HOTSPOT_CONFIG=/path/to/other.config
#
# Override I/O (defaults depend on EVAL_TIER):
#   INTEGRATED_ROOT=.../m5out_eval_xl
#   STANDALONE_ROOT=.../standalone_hotspot_out_xl
#
# Native HotSpot text output is often low precision (~2 decimal places in .ttrace);
# small quantization error vs high-precision integrated CSV is expected.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../../.." && pwd)"

HOTSPOT_BIN="${HOTSPOT_BIN:-}"
RUN_WARM="${RUN_WARM:-0}"

if [[ -z "${HOTSPOT_BIN}" ]]; then
    echo "ERROR: HOTSPOT_BIN is not set."
    echo "Example:"
    echo "  HOTSPOT_BIN=/path/to/HotSpot/hotspot ${SCRIPT_DIR}/run_standalone_hotspot.sh"
    exit 1
fi

if [[ ! -x "${HOTSPOT_BIN}" ]]; then
    echo "ERROR: HOTSPOT_BIN is not executable: ${HOTSPOT_BIN}"
    exit 1
fi

CONVERTER="${SCRIPT_DIR}/gem5_powertrace_to_ptrace.py"
FLP="${SCRIPT_DIR}/hotspot/arm_a9_runtime.flp"

EVAL_TIER="${EVAL_TIER:-large}"
case "${EVAL_TIER}" in
    large)
        _def_integrated="${REPO_ROOT}/m5out_eval_large"
        _def_standalone="${SCRIPT_DIR}/standalone_hotspot_out"
        _def_hotspot_cfg="${SCRIPT_DIR}/hotspot/arm_a9_runtime_block_500000.config"
        ;;
    xl)
        _def_integrated="${REPO_ROOT}/m5out_eval_xl"
        _def_standalone="${SCRIPT_DIR}/standalone_hotspot_out_xl"
        _def_hotspot_cfg="${SCRIPT_DIR}/hotspot/arm_a9_runtime_block_5000000.config"
        ;;
    *)
        echo "ERROR: EVAL_TIER must be 'large' or 'xl' (got: ${EVAL_TIER})" >&2
        exit 1
        ;;
esac

IN_ROOT="${INTEGRATED_ROOT:-${_def_integrated}}"
OUT_ROOT="${STANDALONE_ROOT:-${_def_standalone}}"
CFG="${HOTSPOT_CONFIG:-${_def_hotspot_cfg}}"
PTRACE_ROOT="${OUT_ROOT}/ptraces"

mkdir -p "${OUT_ROOT}" "${PTRACE_ROOT}"

run_one () {
    local app="$1"
    local in_csv="${IN_ROOT}/${app}/power_trace.csv"
    local ptrace="${PTRACE_ROOT}/${app}.ptrace"
    local ttrace="${OUT_ROOT}/${app}.ttrace"
    local steady="${OUT_ROOT}/${app}.steady"
    local init="${OUT_ROOT}/${app}.init"
    local ttrace_warm="${OUT_ROOT}/${app}_warm.ttrace"

    if [[ ! -f "${in_csv}" ]]; then
        echo "ERROR: missing input power trace: ${in_csv}"
        exit 1
    fi

    echo "============================================================"
    echo "Standalone HotSpot for ${app}"
    echo "  HotSpot config : ${CFG}"
    echo "  input csv      : ${in_csv}"
    echo "  ptrace         : ${ptrace}"
    echo "  cold ttrace    : ${ttrace}"
    echo "============================================================"

    # Drop the final flushed partial interval because HotSpot .ptrace assumes a
    # fixed sampling interval for all rows.
    python3 "${CONVERTER}" \
        --input "${in_csv}" \
        --output "${ptrace}" \
        --drop-final-tail \
        --print-summary

    # Cold start: same as integrated (init_temp / ambient from .config)
    "${HOTSPOT_BIN}" \
        -c "${CFG}" \
        -f "${FLP}" \
        -p "${ptrace}" \
        -o "${ttrace}"

    /bin/cp -f "${ttrace}" "${OUT_ROOT}/${app}_cold.ttrace"

    if [[ "${RUN_WARM}" == "1" ]]; then
        echo "RUN_WARM=1: additional steady + warm transient -> ${ttrace_warm}"
        "${HOTSPOT_BIN}" \
            -c "${CFG}" \
            -f "${FLP}" \
            -p "${ptrace}" \
            -steady_file "${steady}"

        cp "${steady}" "${init}"

        "${HOTSPOT_BIN}" \
            -c "${CFG}" \
            -init_file "${init}" \
            -f "${FLP}" \
            -p "${ptrace}" \
            -o "${ttrace_warm}"
    fi
}

run_one saxpy
run_one iaxpy
run_one daxpy

echo
echo "Standalone HotSpot runs complete.  (EVAL_TIER=${EVAL_TIER})"
echo "Input power traces: ${IN_ROOT}/<app>/power_trace.csv"
echo "Outputs: ${OUT_ROOT}"
echo "Validation should use cold <app>.ttrace (not _warm)."
