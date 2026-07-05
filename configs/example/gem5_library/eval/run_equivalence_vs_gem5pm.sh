#!/usr/bin/env bash
# Equivalence check: gem5 (ported) vs gem5-pm reference.
#
# Setup (both sides):
#   PrivateL1SharedL2CacheHierarchy (32kB L1I/L1D, 1MB shared L2)
#   + l1l2_cache_pm power models, InorderMcPAT CPU PM, HotSpot thermal
#   ROI hypercalls 1999/2000 on saxpy/daxpy/iaxpy
#
# Workloads: binaries from gem5-pm/configs/example/gem5_library/workloads/
# Intervals: 250000000 ticks (= 500k cycles @ 2 GHz, matches m5out_eval_large)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GEM5_ROOT="$(cd "${SCRIPT_DIR}/../../../.." && pwd)"
GEM5PM_ROOT="${GEM5PM_ROOT:-$(cd "${GEM5_ROOT}/../gem5-pm" && pwd)}"

GEM5_BIN="${GEM5_ROOT}/build/ARM/gem5.opt"
GEM5PM_BIN="${GEM5PM_ROOT}/build/ARM/gem5.opt"
CFG_GEM5="${GEM5_ROOT}/configs/example/gem5_library/arm_a9_with_mcpat_and_hotspot.py"
CFG_GEM5PM="${GEM5PM_ROOT}/configs/example/gem5_library/arm_a9_with_mcpat_and_hotspot.py"
WORKLOAD_DIR="${GEM5PM_ROOT}/configs/example/gem5_library/workloads"
COMPARE="${SCRIPT_DIR}/compare_traces.py"

OUT_GEM5="${GEM5_ROOT}/m5out_equiv_gem5"
OUT_GEM5PM="${GEM5PM_ROOT}/m5out_equiv_gem5pm"
OUT_GEM5PM_FALLBACK="${GEM5PM_ROOT}/m5out_eval_large"

INTERVAL_TICKS=250000000
RUN_GEM5PM="${RUN_GEM5PM:-0}"

run_one() {
    local bin="$1"
    local cfg="$2"
    local out_root="$3"
    local app="$4"
    local n="$5"
    local repeats="$6"
    local workload_dir_arg="$7"
    local outdir="${out_root}/${app}"

    echo ">>> ${bin##*/} ${app} N=${n} repeats=${repeats} -> ${outdir}"
    "${bin}" -d "${outdir}" "${cfg}" \
        --cpu_type timing \
        --workload "${app}" \
        --workload-args "${n}" "${repeats}" \
        ${workload_dir_arg} \
        --power_interval_ticks "${INTERVAL_TICKS}" \
        --thermal_interval_ticks "${INTERVAL_TICKS}" \
        --power_trace_debug \
        --thermal_trace_debug \
        --hotspot_ambient_temp_k 300.0 \
        --hotspot_initial_temp_k 300.0
}

mkdir -p "${OUT_GEM5}" "${OUT_GEM5PM}"

declare -a APPS=(saxpy iaxpy daxpy)
declare -a NS=(262144 262144 131072)
declare -a REPS=(1000 1000 1000)

for i in "${!APPS[@]}"; do
    app="${APPS[$i]}"
    n="${NS[$i]}"
    reps="${REPS[$i]}"

    if [[ "${RUN_GEM5PM}" == "1" ]]; then
        run_one "${GEM5PM_BIN}" "${CFG_GEM5PM}" "${OUT_GEM5PM}" \
            "${app}" "${n}" "${reps}" ""
    fi

    run_one "${GEM5_BIN}" "${CFG_GEM5}" "${OUT_GEM5}" \
        "${app}" "${n}" "${reps}" \
        "--workload_dir ${WORKLOAD_DIR}"
done

echo
echo "==================== COMPARISON ===================="
for i in "${!APPS[@]}"; do
    app="${APPS[$i]}"
    g5_dir="${OUT_GEM5}/${app}"
    if [[ "${RUN_GEM5PM}" == "1" ]]; then
        pm_dir="${OUT_GEM5PM}/${app}"
    elif [[ -d "${OUT_GEM5PM_FALLBACK}/${app}" ]]; then
        pm_dir="${OUT_GEM5PM_FALLBACK}/${app}"
        echo "(gem5-pm reference: m5out_eval_large/${app})"
    else
        echo "ERROR: no gem5-pm results for ${app}; set RUN_GEM5PM=1"
        continue
    fi
    echo
    echo "======== ${app} ========"
    python3 "${COMPARE}" "${g5_dir}" "${pm_dir}" \
        --label-a gem5 --label-b gem5-pm --skip-first --skip-last
    for stat in simInsts simTicks; do
        g5v=$(grep -m1 "^${stat} " "${g5_dir}/stats.txt" | awk '{print $2}')
        pmv=$(grep -m1 "^${stat} " "${pm_dir}/stats.txt" | awk '{print $2}')
        echo "  ${stat}: gem5=${g5v} gem5-pm=${pmv}"
    done
done
