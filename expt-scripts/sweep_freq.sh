#!/usr/bin/env bash
#
# sweep_freq.sh  – run GEMM at several accelerator clock frequencies
#
#   Usage:  ./sweep_freq.sh
#
#   Prerequisites
#     • $M5_PATH/tools/run_system.sh in $PATH
#     • The tree is under git **or** you do not mind the file
#       configs/SALAM/HwAccConfig.py being edited in-place.
#
#   The script:
#     1. Rewrites the assignment
#            acc.llvm_interface.clock_period = …
#        in  configs/SALAM/HwAccConfig.py  so that
#            clock_period = 1 / freq    (freq in GHz → period in ns).
#     2. Executes the benchmark:
#            $M5_PATH/tools/run_system.sh --bench gemm \
#                --bench-path benchmarks/sys_validation/gemm \
#                --outdir gemm_32x32_<freq>
#     3. Repeats for the frequency sweep
#        {0.1 GHz, 0.5 GHz, 1 GHz, 2 GHz, 5 GHz, 10 GHz, 25 GHz, 50 GHz, 100 GHz}.
#

set -euo pipefail

CFG_FILE="configs/SALAM/HWAccConfig.py"
BENCH_PATH="benchmarks/sys_validation/gemm"
FREQS=(0.1 0.2 0.5 1 2 4 8 16 32 64)

for FREQ in "${FREQS[@]}"; do
    # 1 / freq  (GHz⁻¹ → ns)
    PERIOD=$(python3 - <<EOF
freq = $FREQ
print(f"{1.0 / freq:.6f}")
EOF
)

    echo ">>> Setting frequency = ${FREQ} GHz  (period = ${PERIOD} ns)"

    # Replace the first occurrence that starts with “acc.llvm_interface.clock_period”
    # and rewrite the ENTIRE line.
    sed -i -E \
        "0,/^[[:space:]]*acc\.llvm_interface\.clock_period[[:space:]]*=.*/s//    acc.llvm_interface.clock_period = ${PERIOD}/" \
        "$CFG_FILE"


    OUTDIR="gemm_512x512_${FREQ}"
    "$M5_PATH/tools/run_system.sh" \
        --bench gemm \
        --bench-path "$BENCH_PATH" \
        --outdir "$OUTDIR" \
	--print

    echo ">>> Finished run in ${OUTDIR}"
    echo
done
