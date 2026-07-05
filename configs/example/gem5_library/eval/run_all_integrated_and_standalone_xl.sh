#!/usr/bin/env bash
# XL evaluation: m5out_eval_xl + standalone_hotspot_out_xl, 5M-cycle interval.
# See run_all_integrated_and_standalone.sh header for env vars.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export EVAL_TIER=xl
exec "${SCRIPT_DIR}/run_all_integrated_and_standalone.sh" "$@"
