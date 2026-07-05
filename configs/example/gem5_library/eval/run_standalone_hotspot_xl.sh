#!/usr/bin/env bash
# XL standalone HotSpot: reads m5out_eval_xl, writes standalone_hotspot_out_xl.
# See run_standalone_hotspot.sh for HOTSPOT_BIN, INTEGRATED_ROOT, STANDALONE_ROOT, etc.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export EVAL_TIER=xl
exec "${SCRIPT_DIR}/run_standalone_hotspot.sh" "$@"
