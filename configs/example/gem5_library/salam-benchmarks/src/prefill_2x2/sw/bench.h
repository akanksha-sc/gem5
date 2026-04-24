#ifndef DEFINES
#include "../defines.h"

#endif

extern volatile int stage;

typedef struct
{
    act_t *act;
    weight_t *wgt;
    acc_t *out;
    acc_t *ref;
    uint32_t M;
    uint32_t N;
    uint32_t K;
} prefill_struct;

static inline void
genData(prefill_struct *p)
{
    uint32_t i, j;

    for (i = 0; i < p->M; i++) {
        for (j = 0; j < p->K; j++) {
            p->act[i * p->K + j] = (act_t)(((i + j) % 7) - 3);
        }
    }

    for (i = 0; i < p->K; i++) {
        for (j = 0; j < p->N; j++) {
            p->wgt[i * p->N + j] = (weight_t)(((2 * i + j) % 5) - 2);
        }
    }

    for (i = 0; i < p->M * p->N; i++) {
        p->out[i] = 0;
        p->ref[i] = 0;
    }
}
