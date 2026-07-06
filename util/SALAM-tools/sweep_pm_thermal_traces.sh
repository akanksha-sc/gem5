#!/bin/bash
# Collect example power + thermal traces for all sys_validation + mobilenetv2 benches.
set -uo pipefail

M5_PATH="${M5_PATH:-/nobackup/akankshac/research/gem5}"
ACC_BENCH_PATH="${ACC_BENCH_PATH:-/nobackup/akankshac/research/benchmarks}"
GEM5="${M5_PATH}/build/ARM/gem5.opt"
RUNNER="${M5_PATH}/configs/example/gem5_library/salam/run_salam_stdlib.py"
OUT_ROOT="${M5_PATH}/m5out_pm_thermal_sweep"
TIMEOUT_S="${TIMEOUT_S:-900}"

export ACC_BENCH_PATH

COMMON_FLAGS=(
  --salam-power-sampling
  --salam-thermal-sampling
  --salam-power-auto-start
  --salam-thermal-solver hotspot
  --salam-floorplan-geometry area
  --power-interval-cycles 5000
  --thermal-interval-cycles 5000
  --power-trace-debug
  --thermal-trace-debug
  --mem-size 16GB
)

declare -a BENCHES=(
  "bfs|sys_validation/bfs|config.yml"
  "fft|sys_validation/fft|config.yml"
  "gemm|sys_validation/gemm|config.yml"
  "md_knn|sys_validation/md_knn|config.yml"
  "md_grid|sys_validation/md_grid|config.yml"
  "mergesort|sys_validation/mergesort|config.yml"
  "nw|sys_validation/nw|config.yml"
  "spmv|sys_validation/spmv|config.yml"
  "stencil2d|sys_validation/stencil2d|config.yml"
  "stencil3d|sys_validation/stencil3d|config.yml"
  "edge_tracking|sys_validation/edge_tracking|config.yml"
  "harris_non_max|sys_validation/harris_non_max|config.yml"
  "canny_non_max|sys_validation/canny_non_max|config.yml"
  "convolution|sys_validation/convolution|config.yml"
  "elem_matrix|sys_validation/elem_matrix|config.yml"
  "grayscale|sys_validation/grayscale|config.yml"
  "isp|sys_validation/isp|config.yml"
  "mobilenetv2|mobilenetv2|1_config.yml"
)

mkdir -p "$OUT_ROOT"
SUMMARY="${OUT_ROOT}/summary.csv"
echo "bench,status,power_rows,thermal_rows,floorplan,notes" > "$SUMMARY"

run_one() {
  local name="$1" path="$2" cfg="$3"
  local out="${OUT_ROOT}/${name}"
  local kernel="${ACC_BENCH_PATH}/${path}/sw/main.elf"
  local log="${out}/run.log"

  mkdir -p "$out"

  if [[ ! -f "$kernel" ]]; then
    echo "${name},SKIP,0,0,no,missing_elf" >> "$SUMMARY"
    echo "SKIP $name: missing $kernel"
    return
  fi

  if [[ ! -f "${M5_PATH}/configs/SALAM/${name}.py" ]]; then
    echo "${name},SKIP,0,0,no,missing_salam_config" >> "$SUMMARY"
    echo "SKIP $name: missing configs/SALAM/${name}.py"
    return
  fi

  echo "=== RUN $name ==="
  set +e
  timeout "$TIMEOUT_S" "$GEM5" -d "$out" "$RUNNER" \
    --m5-path "$M5_PATH" \
    --acc-bench-path "$ACC_BENCH_PATH" \
    --bench "$name" \
    --bench-path "$path" \
    --config-name "$cfg" \
    --kernel "$kernel" \
    "${COMMON_FLAGS[@]}" \
    > "$log" 2>&1
  local rc=$?
  set -e

  local power_rows=0 thermal_rows=0 floorplan=no notes=""
  if [[ -f "${out}/power_trace.csv" ]]; then
    power_rows=$(($(wc -l < "${out}/power_trace.csv") - 1))
  fi
  if [[ -f "${out}/thermal_trace.csv" ]]; then
    thermal_rows=$(($(wc -l < "${out}/thermal_trace.csv") - 1))
  fi
  if [[ -f "${out}/salam_hotspot_floorplan.json" ]]; then
    floorplan=yes
  fi

  local status=FAIL
  if [[ $rc -eq 0 && $power_rows -gt 0 && $thermal_rows -gt 0 ]]; then
    status=OK
  elif [[ $rc -eq 124 ]]; then
    notes="timeout_${TIMEOUT_S}s"
  elif [[ $rc -ne 0 ]]; then
    notes="exit_${rc}"
    if grep -q "IndexError\|fatal:\|panic:" "$log" 2>/dev/null; then
      notes="${notes}:$(grep -m1 -E 'IndexError|fatal:|panic:' "$log" | tr ',' ';' | cut -c1-80)"
    fi
  elif [[ $power_rows -eq 0 || $thermal_rows -eq 0 ]]; then
    notes="empty_traces"
  fi

  echo "${name},${status},${power_rows},${thermal_rows},${floorplan},${notes}" >> "$SUMMARY"
  echo "DONE $name -> $status (power=$power_rows thermal=$thermal_rows)"
}

for entry in "${BENCHES[@]}"; do
  IFS='|' read -r name path cfg <<< "$entry"
  run_one "$name" "$path" "$cfg"
done

echo "Summary: $SUMMARY"
cat "$SUMMARY"
