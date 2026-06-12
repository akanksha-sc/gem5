#!/bin/bash
# Regenerate SALAM FunctionalUnits.py / InstConfig from benchmark YAML profiles.
#
# Usage:
#   regenerate_fu_profile.sh [--latency 5ns|10ns] [--bench bfs] [--rebuild]
#
# Requires M5_PATH and ACC_BENCH_PATH. After generation, rebuild gem5 unless
# --rebuild is omitted (incremental scons is still required before simulation).

set -euo pipefail

LATENCY="5ns"
BENCH="bfs"
REBUILD=false

while [[ $# -gt 0 ]]; do
  case $1 in
    --latency)
      LATENCY="$2"
      shift 2
      ;;
    --bench)
      BENCH="$2"
      shift 2
      ;;
    --rebuild)
      REBUILD=true
      shift
      ;;
    -h|--help)
      echo "Usage: $0 [--latency 5ns|10ns] [--bench bfs] [--rebuild]"
      exit 0
      ;;
    *)
      echo "Unknown argument: $1" >&2
      exit 1
      ;;
  esac
done

if [[ -z "${M5_PATH:-}" ]]; then
  echo "M5_PATH must be set" >&2
  exit 1
fi
if [[ -z "${ACC_BENCH_PATH:-}" ]]; then
  echo "ACC_BENCH_PATH must be set" >&2
  exit 1
fi

PROFILE_DIR="${ACC_BENCH_PATH}/sys_validation/${BENCH}/configs/hw_interface/functional_units/40nm_model/${LATENCY}/default_profile"
if [[ ! -d "$PROFILE_DIR" ]]; then
  echo "Profile directory not found: $PROFILE_DIR" >&2
  exit 1
fi

echo "Generating FU models from ${PROFILE_DIR}"
python3 "${M5_PATH}/util/SALAM-tools/hw_generator/HWProfileGenerator.py" \
  -b "$BENCH" --latency "$LATENCY" --fu-only

# Sanity-check that profiles are not the integer_adder clone stub.
AREA=$(python3 - <<PY
import yaml, pathlib
p = pathlib.Path("${PROFILE_DIR}/double_multiplier/double_multiplier.yml")
d = yaml.safe_load(p.read_text())
print(d["functional_unit"]["power_model"]["area"])
PY
)
echo "double_multiplier area (${LATENCY}): ${AREA}"
if python3 - <<PY
area = float("${AREA}")
import sys
# Stub epoch clones integer_adder area (~5.98 um^2).
sys.exit(0 if area < 100 else 1)
PY
then
  echo "WARNING: double_multiplier area looks like a stub profile" >&2
fi

if [[ "$REBUILD" == true ]]; then
  echo "Rebuilding gem5 (ARM/gem5.opt)..."
  scons -C "${M5_PATH}" build/ARM/gem5.opt -j"$(nproc)"
fi

echo "Done. FunctionalUnits.py generated from ${LATENCY} profiles (bench=${BENCH})."
