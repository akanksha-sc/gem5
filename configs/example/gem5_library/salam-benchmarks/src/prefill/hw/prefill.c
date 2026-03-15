#include "hw_defines.h"

void
prefill(void)
{
    volatile act_t *act = (act_t *)ACT;
    volatile weight_t *wgt = (weight_t *)WGT;
    volatile acc_t *out = (acc_t *)OUT;

    uint32_t i;
    uint32_t j;
    uint32_t k;

#pragma nounroll
    for (i = 0; i < MAX_M; i++) {
#pragma nounroll
        for (j = 0; j < MAX_N; j++) {
            acc_t acc = 0;

#pragma nounroll
            for (k = 0; k < MAX_K; k++) {
                acc +=
                    ((acc_t)act[i * MAX_K + k]) * ((acc_t)wgt[k * MAX_N + j]);
            }

            out[i * MAX_N + j] = acc;
        }
    }
}
