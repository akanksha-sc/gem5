#!/bin/bash
# Copyright (c) 2025 Akanksha Chaudhari, Matt Sinclair
# All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#
# Stdlib-only sweep over sys_validation benchmarks (and optional MobileNetV2).
# Requires M5_PATH and ACC_BENCH_PATH.
#
# Usage:
#   validate_stdlib.sh              # full runs with --test-metrics
#   validate_stdlib.sh --dry-run    # configuration attach only
#   validate_stdlib.sh --mobilenet  # also run mobilenetv2 configs

set -euo pipefail

if [[ -z "${M5_PATH:-}" || -z "${ACC_BENCH_PATH:-}" ]]; then
  echo "M5_PATH and ACC_BENCH_PATH must be set" >&2
  exit 1
fi

RUN_MOBILENET=False
DRY_RUN=False

while [[ $# -gt 0 ]]; do
  case "$1" in
    --mobilenet)
      RUN_MOBILENET=True
      shift
      ;;
    --dry-run)
      DRY_RUN=True
      shift
      ;;
    *)
      echo "Unknown option: $1 (supported: --dry-run, --mobilenet)" >&2
      exit 1
      ;;
  esac
done

BENCHES=(
  bfs
  fft
  gemm
  md_grid
  md_knn
  mergesort
  nw
  spmv
  stencil2d
  stencil3d
)

for b in "${BENCHES[@]}"; do
  OUTDIR="BM_ARM_OUT/stdlib/$b"
  CMD=(
    "${M5_PATH}/util/SALAM-tools/run_system_stdlib.sh"
    "--bench" "$b"
    "--bench-path" "sys_validation/$b"
    "--outdir" "$OUTDIR"
  )
  if [[ "$DRY_RUN" == True ]]; then
    CMD+=("--dry-run")
  else
    CMD+=("--test-metrics")
  fi
  echo "=== stdlib $b ==="
  "${CMD[@]}"
done

if [[ "$RUN_MOBILENET" == True ]]; then
  for cfg in 35_config.yml 75_config.yml 1_config.yml; do
    tag="${cfg%_config.yml}"
    OUTDIR="BM_ARM_OUT/stdlib/mobilenetv2_$tag"
    CMD=(
      "${M5_PATH}/util/SALAM-tools/run_system_stdlib.sh"
      "--bench" "mobilenetv2"
      "--bench-path" "mobilenetv2"
      "--config-name" "$cfg"
      "--outdir" "$OUTDIR"
    )
    if [[ "$DRY_RUN" == True ]]; then
      CMD+=("--dry-run")
    else
      CMD+=("--test-metrics")
    fi
    echo "=== stdlib mobilenetv2 $cfg ==="
    "${CMD[@]}"
  done
fi
