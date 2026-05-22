#!/bin/bash
# Copyright (c) 2025 Akanksha Chaudhari, Matt Sinclair
# All rights reserved.
#
# This file contains modifications and/or code derived from:
# gem5-SALAM: https://github.com/TeCSAR-UNCC/gem5-SALAM
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
# this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
# this list of conditions and the following disclaimer in the documentation
# and/or other materials provided with the distribution.
#
# 3. Neither the name of the copyright holder nor the names of its
# contributors may be used to endorse or promote products derived from this
# software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

set -euo pipefail

BENCH=""
BENCH_PATH=""
CONFIG_NAME=""
FLAGS=""
BUILD=True
DEBUG=False
PRINT_TO_FILE=False
VALGRIND=False
OUTDIR=""
TEST_METRICS=False
DRY_RUN=False

ACC_CLOCK=""
ACC_VOLTAGE=""
ACC_COMPUTE_CLOCK=""
ACC_COMPUTE_VOLTAGE=""
ACC_MEM_CLOCK=""
ACC_MEM_VOLTAGE=""
ACC_DMA_CLOCK=""
ACC_DMA_VOLTAGE=""
ACC_LOCALBUS_CLOCK=""
ACC_LOCALBUS_VOLTAGE=""

while [[ $# -gt 0 ]]; do
  case $1 in
    --bench)
      BENCH="$2"
      shift
      shift
      ;;
    --bench-path)
      BENCH_PATH="$2"
      shift
      shift
      ;;
    --config-name)
      CONFIG_NAME="$2"
      shift
      shift
      ;;
    -f|--flags)
      FLAGS="$2"
      shift
      shift
      ;;
    -d|--debug)
      DEBUG=True
      shift
      ;;
    -b|--build)
      BUILD=True
      shift
      ;;
    -p|--print)
      PRINT_TO_FILE=True
      shift
      ;;
    -v|--valgrind)
      VALGRIND=True
      shift
      ;;
    --test-metrics)
      TEST_METRICS=True
      shift
      ;;
    --dry-run)
      DRY_RUN=True
      shift
      ;;
    --sys-clock)
      SYS_CLOCK="$2"
      shift
      shift
      ;;
    --acc-clock)
      ACC_CLOCK="$2"
      shift
      shift
      ;;
    --acc-voltage)
      ACC_VOLTAGE="$2"
      shift
      shift
      ;;
    --acc-compute-clock)
      ACC_COMPUTE_CLOCK="$2"
      shift
      shift
      ;;
    --acc-compute-voltage)
      ACC_COMPUTE_VOLTAGE="$2"
      shift
      shift
      ;;
    --acc-mem-clock)
      ACC_MEM_CLOCK="$2"
      shift
      shift
      ;;
    --acc-mem-voltage)
      ACC_MEM_VOLTAGE="$2"
      shift
      shift
      ;;
    --acc-dma-clock)
      ACC_DMA_CLOCK="$2"
      shift
      shift
      ;;
    --acc-dma-voltage)
      ACC_DMA_VOLTAGE="$2"
      shift
      shift
      ;;
    --acc-localbus-clock)
      ACC_LOCALBUS_CLOCK="$2"
      shift
      shift
      ;;
    --acc-localbus-voltage)
      ACC_LOCALBUS_VOLTAGE="$2"
      shift
      shift
      ;;
    --outdir)
      OUTDIR="$2"
      shift
      shift
      ;;
    -*)
      echo "Unknown option $1"
      exit 1
      ;;
    *)
      shift
      ;;
  esac
done

if [[ "$DRY_RUN" == True && "$TEST_METRICS" == True ]]; then
  echo "--test-metrics is not meaningful with --dry-run; disabling metrics"
  TEST_METRICS=False
fi

if [ "$BENCH" == "" ]; then
  echo "BENCH (--bench) is not set, exiting"
  exit 1
fi

if [[ -z "${M5_PATH:-}" ]]; then
  echo "M5_PATH env var is not set, exiting"
  exit 1
fi

if [[ -z "${ACC_BENCH_PATH:-}" ]]; then
  echo "ACC_BENCH_PATH env var is not set, exiting"
  exit 1
fi

if [ "$CONFIG_NAME" == "" ]; then
  CONFIG_NAME="config.yml"
fi

if [ "$BENCH_PATH" == "" ]; then
  BENCH_PATH=$BENCH
fi

if [ -z "${OUTDIR:-}" ]; then
  OUTDIR="BM_ARM_OUT/$BENCH_PATH/"
fi

KERNEL="${ACC_BENCH_PATH}/${BENCH_PATH}/sw/main.elf"

# Gem5 command as an array (safe quoting for unusual paths).
# No --disk-image is passed here. The stdlib SALAM path is bare-metal and
# uses --kernel to point directly at sw/main.elf.
GEM5_CMD=()
if [[ "$DEBUG" == True ]]; then
  GEM5_CMD=(gdb --args "${M5_PATH}/build/ARM/gem5.debug")
elif [[ "$VALGRIND" == True ]]; then
  GEM5_CMD=(
    valgrind
    --leak-check=yes
    --suppressions="${M5_PATH}/util/valgrind-suppressions"
    --suppressions="${M5_PATH}/util/salam.supp"
    --track-origins=yes
    --error-limit=no
    --leak-check=full
    --show-leak-kinds=definite,possible
    --show-reachable=no
    --log-file="${BENCH}.log"
    "${M5_PATH}/build/ARM/gem5.debug"
  )
else
  GEM5_CMD=("${M5_PATH}/build/ARM/gem5.opt")
fi

if [[ -n "$FLAGS" ]]; then
  GEM5_CMD+=("--debug-flags=${FLAGS}")
fi

GEM5_CMD+=(
  "--outdir=$OUTDIR"
  "${M5_PATH}/configs/example/gem5_library/salam/run_salam_stdlib.py"
  "--m5-path" "$M5_PATH"
  "--acc-bench-path" "$ACC_BENCH_PATH"
  "--bench" "$BENCH"
  "--bench-path" "$BENCH_PATH"
  "--config-name" "$CONFIG_NAME"
  "--kernel" "$KERNEL"
  "--mem-size" "16GB"
)

if [[ -n "${SYS_CLOCK:-}" ]]; then
  GEM5_CMD+=("--sys-clock" "$SYS_CLOCK")
fi

if [[ -n "$ACC_CLOCK" ]]; then
  GEM5_CMD+=("--acc-clock" "$ACC_CLOCK")
fi
if [[ -n "$ACC_VOLTAGE" ]]; then
  GEM5_CMD+=("--acc-voltage" "$ACC_VOLTAGE")
fi
if [[ -n "$ACC_COMPUTE_CLOCK" ]]; then
  GEM5_CMD+=("--acc-compute-clock" "$ACC_COMPUTE_CLOCK")
fi
if [[ -n "$ACC_COMPUTE_VOLTAGE" ]]; then
  GEM5_CMD+=("--acc-compute-voltage" "$ACC_COMPUTE_VOLTAGE")
fi
if [[ -n "$ACC_MEM_CLOCK" ]]; then
  GEM5_CMD+=("--acc-mem-clock" "$ACC_MEM_CLOCK")
fi
if [[ -n "$ACC_MEM_VOLTAGE" ]]; then
  GEM5_CMD+=("--acc-mem-voltage" "$ACC_MEM_VOLTAGE")
fi
if [[ -n "$ACC_DMA_CLOCK" ]]; then
  GEM5_CMD+=("--acc-dma-clock" "$ACC_DMA_CLOCK")
fi
if [[ -n "$ACC_DMA_VOLTAGE" ]]; then
  GEM5_CMD+=("--acc-dma-voltage" "$ACC_DMA_VOLTAGE")
fi
if [[ -n "$ACC_LOCALBUS_CLOCK" ]]; then
  GEM5_CMD+=("--acc-localbus-clock" "$ACC_LOCALBUS_CLOCK")
fi
if [[ -n "$ACC_LOCALBUS_VOLTAGE" ]]; then
  GEM5_CMD+=("--acc-localbus-voltage" "$ACC_LOCALBUS_VOLTAGE")
fi

if [[ "$DRY_RUN" == True ]]; then
  GEM5_CMD+=("--dry-run")
fi

if ! "$M5_PATH/util/SALAM-tools/SALAM-Configurator/systembuilder.py" \
    --sys-name "$BENCH" \
    --bench-path "$BENCH_PATH" \
    --config-name "$CONFIG_NAME" \
    --m5-path "$M5_PATH" \
    --acc-bench-path "$ACC_BENCH_PATH" \
    --no-fs-template \
    --emit-manifest; then
  echo "Configurator failed"
  exit 1
fi

if [[ "$BUILD" == True ]]; then
  echo "Building Bench"
  make all -C "$ACC_BENCH_PATH/$BENCH_PATH"
fi

if [[ ! -f "$KERNEL" ]]; then
  echo "Kernel/bare-metal ELF not found: $KERNEL" >&2
  exit 1
fi

mkdir -p "$OUTDIR"

if [[ "$PRINT_TO_FILE" == True || "$TEST_METRICS" == True ]]; then
  "${GEM5_CMD[@]}" > "${OUTDIR}/debug-trace.txt"
else
  "${GEM5_CMD[@]}"
fi

if [[ "$TEST_METRICS" == True ]]; then
  python3 "$M5_PATH/util/SALAM-tools/test_metrics.py" "$OUTDIR" \
    | tee "${OUTDIR}/test.log"
fi
