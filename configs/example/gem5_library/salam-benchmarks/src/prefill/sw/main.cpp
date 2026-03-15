#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../../common/m5ops.h"
#include "../prefill_clstr_hw_defines.h"
#include "bench.h"

prefill_struct prefill_data;

#define BASE 0x80c00000

#define ACT_OFFSET 0
#define WGT_OFFSET (ACT_OFFSET + sizeof(act_t) * MAX_M * MAX_K)
#define OUT_OFFSET (WGT_OFFSET + sizeof(weight_t) * MAX_K * MAX_N)
#define REF_OFFSET (OUT_OFFSET + sizeof(acc_t) * MAX_M * MAX_N)

volatile uint8_t *top = (uint8_t *)(TOP);
volatile uint32_t *ACT_ADDR = (uint32_t *)(TOP + 1);
volatile uint32_t *WGT_ADDR = (uint32_t *)(TOP + 9);
volatile uint32_t *OUT_ADDR = (uint32_t *)(TOP + 17);

volatile int stage = 0;

static void
reference_gemm(prefill_struct *p)
{
    uint32_t i, j, k;

    for (i = 0; i < p->M; i++) {
        for (j = 0; j < p->N; j++) {
            acc_t acc = 0;
            for (k = 0; k < p->K; k++) {
                acc += ((acc_t)p->act[i * p->K + k]) *
                       ((acc_t)p->wgt[k * p->N + j]);
            }
            p->ref[i * p->N + j] = acc;
        }
    }
}

int
main(void)
{
    act_t *act = (act_t *)(BASE + ACT_OFFSET);
    weight_t *wgt = (weight_t *)(BASE + WGT_OFFSET);
    acc_t *out = (acc_t *)(BASE + OUT_OFFSET);
    acc_t *ref = (acc_t *)(BASE + REF_OFFSET);

    stage = 0;
    volatile int count = 0;

    prefill_data.act = act;
    prefill_data.wgt = wgt;
    prefill_data.out = out;
    prefill_data.ref = ref;
    prefill_data.M = MAX_M;
    prefill_data.N = MAX_N;
    prefill_data.K = MAX_K;

    printf("Generating data\n");
    genData(&prefill_data);
    printf("Data generated\n");

#ifdef CHECK
    printf("Running software reference\n");
    reference_gemm(&prefill_data);
#endif

    *ACT_ADDR = (uint64_t)(uintptr_t)act;
    *WGT_ADDR = (uint64_t)(uintptr_t)wgt;
    *OUT_ADDR = (uint64_t)(uintptr_t)out;

    printf("Starting job\n");
    *top = 0x01;

    while (stage < 1) {
        count++;
    }

    printf("Job complete\n");

#ifdef CHECK
    bool fail = false;

    for (uint32_t i = 0; i < MAX_M; i++) {
        for (uint32_t j = 0; j < MAX_N; j++) {
            acc_t got = out[i * MAX_N + j];
            acc_t exp = ref[i * MAX_N + j];
            if (got != exp) {
                fail = true;
                printf("Mismatch at (%u,%u): %d found, %d expected\n", i, j,
                       got, exp);
            }
        }
    }

    if (fail) {
        printf("Check Failed\n");
    } else {
        printf("Check Passed\n");
    }
#endif

    m5_dump_stats();
    m5_exit();
}
