#!/bin/bash
# Long-running mobilenetv2 SALAM trace collection (no timeout).
# Phase 1: power trace only
# Phase 2: power + thermal trace
#
# Progress is written to <outdir>/progress.log every MONITOR_INTERVAL_S seconds
# and echoed to the main log via tee.
set -uo pipefail

M5_PATH="${M5_PATH:-/nobackup/akankshac/research/gem5}"
ACC_BENCH_PATH="${ACC_BENCH_PATH:-/nobackup/akankshac/research/benchmarks}"
GEM5="${M5_PATH}/build/ARM/gem5.opt"
RUNNER="${M5_PATH}/configs/example/gem5_library/salam/run_salam_stdlib.py"
OUT_ROOT="${OUT_ROOT:-${M5_PATH}/m5out_mobilenetv2}"
MONITOR_INTERVAL_S="${MONITOR_INTERVAL_S:-30}"
PHASE="${PHASE:-both}"  # power | thermal | both

export ACC_BENCH_PATH

KERNEL="${ACC_BENCH_PATH}/mobilenetv2/sw/main.elf"
COMMON=(
  --m5-path "$M5_PATH"
  --acc-bench-path "$ACC_BENCH_PATH"
  --bench mobilenetv2
  --bench-path mobilenetv2
  --config-name 1_config.yml
  --kernel "$KERNEL"
  --acc-compute-clock 100MHz
  --mem-size 16GB
  --salam-power-sampling
  --salam-power-auto-start
  --power-interval-cycles 5000
  --power-trace-debug
)

monitor_run() {
  local outdir="$1" phase="$2" gem5_pid="$3"
  local progress="${outdir}/progress.log"
  local start_ts
  start_ts=$(date +%s)
  echo "[monitor] phase=${phase} pid=${gem5_pid} outdir=${outdir}" | tee -a "$progress"

  while kill -0 "$gem5_pid" 2>/dev/null; do
    local now elapsed power_rows thermal_rows floorplan last_line
    now=$(date -Iseconds)
    elapsed=$(( $(date +%s) - start_ts ))
    power_rows=0
    thermal_rows=0
    floorplan=no
    [[ -f "${outdir}/power_trace.csv" ]] && \
      power_rows=$(($(wc -l < "${outdir}/power_trace.csv") - 1))
    [[ -f "${outdir}/thermal_trace.csv" ]] && \
      thermal_rows=$(($(wc -l < "${outdir}/thermal_trace.csv") - 1))
    [[ -f "${outdir}/salam_hotspot_floorplan.json" ]] && floorplan=yes
    last_line=$(grep -E 'SALAM_SUMMARY|simTicks|Exiting @|End Simulation' \
      "${outdir}/run.log" 2>/dev/null | tail -1 | cut -c1-120)
    printf '%s phase=%s elapsed=%ds power_rows=%d thermal_rows=%d floorplan=%s %s\n' \
      "$now" "$phase" "$elapsed" "$power_rows" "$thermal_rows" "$floorplan" \
      "${last_line:-}" | tee -a "$progress"
    sleep "$MONITOR_INTERVAL_S" || break
  done

  set +e
  wait "$gem5_pid"
  local rc=$?
  set -e
  local status=FAIL notes=""
  local power_rows=0 thermal_rows=0 floorplan=no
  [[ -f "${outdir}/power_trace.csv" ]] && \
    power_rows=$(($(wc -l < "${outdir}/power_trace.csv") - 1))
  [[ -f "${outdir}/thermal_trace.csv" ]] && \
    thermal_rows=$(($(wc -l < "${outdir}/thermal_trace.csv") - 1))
  [[ -f "${outdir}/salam_hotspot_floorplan.json" ]] && floorplan=yes

  if [[ $rc -eq 0 && $power_rows -gt 0 ]]; then
    if [[ "$phase" == "power" ]] || [[ $thermal_rows -gt 0 ]]; then
      status=OK
    else
      notes="missing_thermal_trace"
    fi
  else
    notes="exit_${rc}"
    grep -q -E 'fatal:|panic:' "${outdir}/run.log" 2>/dev/null && \
      notes="${notes}:$(grep -m1 -E 'fatal:|panic:' "${outdir}/run.log" | tr ',' ';' | cut -c1-80)"
  fi

  echo "[done] phase=${phase} status=${status} exit=${rc} power_rows=${power_rows} thermal_rows=${thermal_rows} floorplan=${floorplan} notes=${notes}" \
    | tee -a "$progress"
  return $rc
}

run_phase() {
  local name="$1"
  shift
  local outdir="${OUT_ROOT}/${name}"
  mkdir -p "$outdir"
  local log="${outdir}/run.log"
  local progress="${outdir}/progress.log"

  echo "=== START ${name} $(date -Iseconds) ===" | tee "$progress"
  echo "outdir=${outdir}" | tee -a "$progress"
  echo "cmd: $GEM5 -d $outdir $RUNNER ${COMMON[*]} $*" | tee -a "$progress"

  set +e
  "$GEM5" -d "$outdir" "$RUNNER" "${COMMON[@]}" "$@" >>"$log" 2>&1 &
  local gem5_pid=$!
  set -e

  monitor_run "$outdir" "$name" "$gem5_pid"
}

mkdir -p "$OUT_ROOT"
SUMMARY="${OUT_ROOT}/summary.csv"
if [[ "$PHASE" == "thermal" && -f "$SUMMARY" ]]; then
  : # keep existing summary (e.g. power_only row from a prior run)
else
  echo "phase,status,power_rows,thermal_rows,floorplan,notes" > "$SUMMARY"
fi

if [[ "$PHASE" == "power" || "$PHASE" == "both" ]]; then
  run_phase power_only
  rc=$?
  power_rows=0; thermal_rows=0; floorplan=no; status=FAIL; notes=""
  [[ -f "${OUT_ROOT}/power_only/power_trace.csv" ]] && \
    power_rows=$(($(wc -l < "${OUT_ROOT}/power_only/power_trace.csv") - 1))
  [[ -f "${OUT_ROOT}/power_only/salam_hotspot_floorplan.json" ]] && floorplan=yes
  [[ $rc -eq 0 && $power_rows -gt 0 ]] && status=OK || notes="exit_${rc}"
  echo "power_only,${status},${power_rows},0,${floorplan},${notes}" >> "$SUMMARY"
  [[ $rc -ne 0 && "$PHASE" == "both" ]] && exit $rc
fi

if [[ "$PHASE" == "thermal" || "$PHASE" == "both" ]]; then
  run_phase power_thermal \
    --salam-thermal-sampling \
    --salam-thermal-solver hotspot \
    --salam-floorplan-geometry area \
    --thermal-interval-cycles 5000 \
    --thermal-trace-debug
  rc=$?
  power_rows=0; thermal_rows=0; floorplan=no; status=FAIL; notes=""
  [[ -f "${OUT_ROOT}/power_thermal/power_trace.csv" ]] && \
    power_rows=$(($(wc -l < "${OUT_ROOT}/power_thermal/power_trace.csv") - 1))
  [[ -f "${OUT_ROOT}/power_thermal/thermal_trace.csv" ]] && \
    thermal_rows=$(($(wc -l < "${OUT_ROOT}/power_thermal/thermal_trace.csv") - 1))
  [[ -f "${OUT_ROOT}/power_thermal/salam_hotspot_floorplan.json" ]] && floorplan=yes
  [[ $rc -eq 0 && $power_rows -gt 0 && $thermal_rows -gt 0 ]] && status=OK || notes="exit_${rc}"
  echo "power_thermal,${status},${power_rows},${thermal_rows},${floorplan},${notes}" >> "$SUMMARY"
fi

echo "=== ALL DONE $(date -Iseconds) ===" | tee -a "${OUT_ROOT}/master.log"
cat "$SUMMARY"
