#include "hw_defines.h"

static inline void
prefill_signed(uint32_t kdim, volatile act_t *act, volatile weight_t *wgt,
               volatile acc_t *out)
{
    uint32_t i;
    uint32_t j;
    uint32_t k;

#pragma nounroll
    for (i = 0; i < MAX_M; i += MMU_ROWS) {
#pragma nounroll
        for (j = 0; j < MAX_N; j += MMU_COLS) {
            acc_t acc00 = 0;
            acc_t acc01 = 0;
            acc_t acc10 = 0;
            acc_t acc11 = 0;

#pragma nounroll
            for (k = 0; k < kdim; k++) {
                int32_t a0 = (int8_t)act[(i + 0) * MAX_K + k];
                int32_t a1 = (int8_t)act[(i + 1) * MAX_K + k];
                int32_t w0 = (int8_t)wgt[k * MAX_N + (j + 0)];
                int32_t w1 = (int8_t)wgt[k * MAX_N + (j + 1)];

                acc00 += a0 * w0;
                acc01 += a0 * w1;
                acc10 += a1 * w0;
                acc11 += a1 * w1;
            }

            out[(i + 0) * MAX_N + (j + 0)] = acc00;
            out[(i + 0) * MAX_N + (j + 1)] = acc01;
            out[(i + 1) * MAX_N + (j + 0)] = acc10;
            out[(i + 1) * MAX_N + (j + 1)] = acc11;
        }
    }
}

static inline void
prefill_unsigned(uint32_t kdim, volatile act_t *act, volatile weight_t *wgt,
                 volatile acc_t *out)
{
    uint32_t i;
    uint32_t j;
    uint32_t k;

#pragma nounroll
    for (i = 0; i < MAX_M; i += MMU_ROWS) {
#pragma nounroll
        for (j = 0; j < MAX_N; j += MMU_COLS) {
            acc_t acc00 = 0;
            acc_t acc01 = 0;
            acc_t acc10 = 0;
            acc_t acc11 = 0;

#pragma nounroll
            for (k = 0; k < kdim; k++) {
                uint32_t a0 = (uint8_t)act[(i + 0) * MAX_K + k];
                uint32_t a1 = (uint8_t)act[(i + 1) * MAX_K + k];
                uint32_t w0 = (uint8_t)wgt[k * MAX_N + (j + 0)];
                uint32_t w1 = (uint8_t)wgt[k * MAX_N + (j + 1)];

                acc00 += (acc_t)(a0 * w0);
                acc01 += (acc_t)(a0 * w1);
                acc10 += (acc_t)(a1 * w0);
                acc11 += (acc_t)(a1 * w1);
            }

            out[(i + 0) * MAX_N + (j + 0)] = acc00;
            out[(i + 0) * MAX_N + (j + 1)] = acc01;
            out[(i + 1) * MAX_N + (j + 0)] = acc10;
            out[(i + 1) * MAX_N + (j + 1)] = acc11;
        }
    }
}

void
prefill(uint32_t cfg, uint32_t kdim)
{
    volatile act_t *act = (act_t *)ACT;
    volatile weight_t *wgt = (weight_t *)WGT;
    volatile acc_t *out = (acc_t *)OUT;

    /*
     * Reserved for future packed 4b / split-datapath support.
     * Current implementation models the 2x2 MMU structure for the
     * full-width mode only.
     */
    if (cfg & CFG_SIGNED) {
        prefill_signed(kdim, act, wgt, out);
    } else {
        prefill_unsigned(kdim, act, wgt, out);
    }
}
