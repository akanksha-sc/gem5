#!/usr/bin/env bash
set -euo pipefail

# End-to-end evaluation launcher:
#   1) integrated gem5 + McPAT + runtime HotSpot
#   2) convert power_trace.csv -> HotSpot .ptrace
#   3) standalone HotSpot cold-start transient run -> <app>.ttrace
#
# Evaluation tier (does not touch the other tier's output directories):
#   EVAL_TIER=large  (default) -> m5out_eval_large, standalone_hotspot_out, interval 500k
#   EVAL_TIER=xl     -> m5out_eval_xl, standalone_hotspot_out_xl, interval 5M, bigger workloads
# Or: ./run_all_integrated_and_standalone_xl.sh
#
# Override integrated / standalone roots:
#   STANDALONE_ROOT=.../standalone_hotspot_out_xl
# (M5out root is fixed per tier: m5out_eval_large vs m5out_eval_xl)
#
# Sampling: POWER_THERMAL_INTERVAL cycles @ 2 GHz must match
#   hotspot/arm_a9_runtime_block_*.config for that interval (-sampling_intvl).
# XL: arm_a9_runtime_block_5000000.config (2.5e-3 s). For coarser 10M cycles use
#   POWER_THERMAL_INTERVAL=10000000 and HOTSPOT_CONFIG=.../arm_a9_runtime_block_10000000.config
#
# For a fair validation vs integrated traces, standalone must be COLD start
# (no -init_file from steady). consolidate_validation_results.py expects:
#   standalone_hotspot_out/<app>.ttrace
#
# Optional:
#   RUN_WARM=1 HOTSPOT_BIN=/path/to/hotspot ./run_all_integrated_and_standalone.sh
# will also produce steady-state warm-started <app>_warm.ttrace.
#
# Override HotSpot config if needed (e.g. old 50k traces):
#   HOTSPOT_CONFIG=.../arm_a9_runtime_block_50000.config

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../../.." && pwd)"

GEM5_BIN="${REPO_ROOT}/build/ARM/gem5.opt"
CFG="${REPO_ROOT}/configs/example/gem5_library/arm_a9_with_mcpat_and_hotspot.py"

CONVERTER="${SCRIPT_DIR}/gem5_powertrace_to_ptrace.py"
FLP="${SCRIPT_DIR}/hotspot/arm_a9_runtime.flp"

EVAL_TIER="${EVAL_TIER:-large}"
case "${EVAL_TIER}" in
    large)
        # ~10x longer workloads vs 100 repeats; ~similar sample counts with 10x interval.
        _default_interval=500000
        OUT_ROOT="${REPO_ROOT}/m5out_eval_large"
        _def_standalone="${SCRIPT_DIR}/standalone_hotspot_out"
        _def_hotspot_cfg="${SCRIPT_DIR}/hotspot/arm_a9_runtime_block_500000.config"
        ;;
    xl)
        # 4x more work vs large, 10x sampling interval: stronger heating, fewer rows.
        _default_interval=5000000
        OUT_ROOT="${REPO_ROOT}/m5out_eval_xl"
        _def_standalone="${SCRIPT_DIR}/standalone_hotspot_out_xl"
        _def_hotspot_cfg="${SCRIPT_DIR}/hotspot/arm_a9_runtime_block_5000000.config"
        ;;
    *)
        echo "ERROR: EVAL_TIER must be 'large' or 'xl' (got: ${EVAL_TIER})" >&2
        exit 1
        ;;
esac

POWER_THERMAL_INTERVAL="${POWER_THERMAL_INTERVAL:-${_default_interval}}"
HS_OUT_ROOT="${STANDALONE_ROOT:-${_def_standalone}}"
CFG_HOTSPOT="${HOTSPOT_CONFIG:-${_def_hotspot_cfg}}"
PTRACE_ROOT="${HS_OUT_ROOT}/ptraces"

HOTSPOT_BIN="${HOTSPOT_BIN:-}"
RUN_WARM="${RUN_WARM:-0}"

if [[ ! -x "${GEM5_BIN}" ]]; then
    echo "ERROR: gem5 binary not found or not executable: ${GEM5_BIN}"
    exit 1
fi

if [[ ! -f "${CFG}" ]]; then
    echo "ERROR: config script not found: ${CFG}"
    exit 1
fi

if [[ ! -f "${CONVERTER}" ]]; then
    echo "ERROR: converter script not found: ${CONVERTER}"
    exit 1
fi

if [[ ! -f "${FLP}" ]]; then
    echo "ERROR: HotSpot floorplan not found: ${FLP}"
    exit 1
fi

if [[ ! -f "${CFG_HOTSPOT}" ]]; then
    echo "ERROR: HotSpot config not found: ${CFG_HOTSPOT}"
    exit 1
fi

if [[ -z "${HOTSPOT_BIN}" ]]; then
    echo "ERROR: HOTSPOT_BIN is not set."
    echo "Example:"
    echo "  HOTSPOT_BIN=/path/to/HotSpot/hotspot \\"
    echo "  configs/example/gem5_library/eval/run_all_integrated_and_standalone.sh"
    exit 1
fi

if [[ ! -x "${HOTSPOT_BIN}" ]]; then
    echo "ERROR: HOTSPOT_BIN is not executable: ${HOTSPOT_BIN}"
    exit 1
fi

mkdir -p "${OUT_ROOT}" "${HS_OUT_ROOT}" "${PTRACE_ROOT}"

run_integrated_one () {
    local app="$1"
    local n="$2"
    local repeats="$3"
    local outdir="${OUT_ROOT}/${app}"

    echo
    echo "============================================================"
    echo "Integrated runtime run: ${app}"
    echo "  output dir       : ${outdir}"
    echo "  workload args    : ${n} ${repeats}"
    echo "  power interval   : ${POWER_THERMAL_INTERVAL}"
    echo "  thermal interval : ${POWER_THERMAL_INTERVAL}"
    echo "============================================================"

    "${GEM5_BIN}" -d "${outdir}" \
        "${CFG}" \
        --cpu_type timing \
        --workload "${app}" \
        --workload-args "${n}" "${repeats}" \
        --power_interval "${POWER_THERMAL_INTERVAL}" \
        --thermal_interval "${POWER_THERMAL_INTERVAL}" \
        --power_trace_debug \
        --thermal_trace_debug \
        --hotspot_ambient_temp_k 300.0 \
        --hotspot_initial_temp_k 300.0
}

convert_ptrace_one () {
    local app="$1"
    local in_csv="${OUT_ROOT}/${app}/power_trace.csv"
    local ptrace="${PTRACE_ROOT}/${app}.ptrace"

    if [[ ! -f "${in_csv}" ]]; then
        echo "ERROR: missing input power trace: ${in_csv}"
        exit 1
    fi

    echo
    echo "------------------------------------------------------------"
    echo "Converting power trace for ${app}"
    echo "  input : ${in_csv}"
    echo "  output: ${ptrace}"
    echo "------------------------------------------------------------"

    # Drop final flushed tail sample because standalone HotSpot expects one
    # fixed sampling interval for all rows.
    python3 "${CONVERTER}" \
        --input "${in_csv}" \
        --output "${ptrace}" \
        --drop-final-tail \
        --print-summary
}

run_standalone_cold_one () {
    local app="$1"
    local ptrace="${PTRACE_ROOT}/${app}.ptrace"
    local ttrace_main="${HS_OUT_ROOT}/${app}.ttrace"
    local ttrace_cold="${HS_OUT_ROOT}/${app}_cold.ttrace"

    if [[ ! -f "${ptrace}" ]]; then
        echo "ERROR: missing ptrace: ${ptrace}"
        exit 1
    fi

    echo
    echo "------------------------------------------------------------"
    echo "Standalone HotSpot cold-start run: ${app}"
    echo "  config : ${CFG_HOTSPOT}"
    echo "  ptrace : ${ptrace}"
    echo "  ttrace : ${ttrace_main} (and copy -> ${ttrace_cold})"
    echo "------------------------------------------------------------"

    "${HOTSPOT_BIN}" \
        -c "${CFG_HOTSPOT}" \
        -f "${FLP}" \
        -p "${ptrace}" \
        -o "${ttrace_main}"

    /bin/cp -f "${ttrace_main}" "${ttrace_cold}"
}

run_standalone_warm_one () {
    local app="$1"
    local ptrace="${PTRACE_ROOT}/${app}.ptrace"
    local steady="${HS_OUT_ROOT}/${app}.steady"
    local init="${HS_OUT_ROOT}/${app}.init"
    local ttrace="${HS_OUT_ROOT}/${app}_warm.ttrace"

    if [[ ! -f "${ptrace}" ]]; then
        echo "ERROR: missing ptrace: ${ptrace}"
        exit 1
    fi

    echo
    echo "------------------------------------------------------------"
    echo "Standalone HotSpot warm-start run: ${app}"
    echo "  ptrace : ${ptrace}"
    echo "  steady : ${steady}"
    echo "  ttrace : ${ttrace}"
    echo "------------------------------------------------------------"

    "${HOTSPOT_BIN}" \
        -c "${CFG_HOTSPOT}" \
        -f "${FLP}" \
        -p "${ptrace}" \
        -steady_file "${steady}"

    cp "${steady}" "${init}"

    "${HOTSPOT_BIN}" \
        -c "${CFG_HOTSPOT}" \
        -init_file "${init}" \
        -f "${FLP}" \
        -p "${ptrace}" \
        -o "${ttrace}"
}

run_one () {
    local app="$1"
    local n="$2"
    local repeats="$3"

    run_integrated_one "${app}" "${n}" "${repeats}"
    convert_ptrace_one "${app}"
    run_standalone_cold_one "${app}"

    if [[ "${RUN_WARM}" == "1" ]]; then
        run_standalone_warm_one "${app}"
    fi
}

# Workload sizes: large vs XL (separate m5out trees — no clobbering)
if [[ "${EVAL_TIER}" == "xl" ]]; then
    run_one saxpy 524288 2000
    run_one iaxpy 524288 2000
    run_one daxpy 262144 2000
else
    # saxpy / iaxpy: larger vector, 1000 repeats; daxpy: half vector, 1000 repeats
    run_one saxpy 262144 1000
    run_one iaxpy 262144 1000
    run_one daxpy 131072 1000
fi

echo
echo "============================================================"
echo "All integrated + standalone runs complete.  (EVAL_TIER=${EVAL_TIER})"
echo
echo "Integrated outputs:"
echo "  ${OUT_ROOT}/saxpy"
echo "  ${OUT_ROOT}/iaxpy"
echo "  ${OUT_ROOT}/daxpy"
echo
echo "Standalone HotSpot outputs:"
echo "  ${HS_OUT_ROOT}"
echo
echo "Cold transient (use for validation vs integrated):"
echo "  <app>.ttrace  (copy also saved as <app>_cold.ttrace)"
echo
echo "If RUN_WARM=1 was set, also produced:"
echo "  <app>.steady"
echo "  <app>.init"
echo "  <app>_warm.ttrace"
echo "============================================================"
