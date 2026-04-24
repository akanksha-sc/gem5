#include "../defines.h"
#include "../prefill_clstr_hw_defines.h"

#define DEV_INIT 0x01
#define DEV_INTR 0x04

#define ACTSIZE (MAX_M * MAX_K * sizeof(act_t))
#define WGTSIZE (MAX_K * MAX_N * sizeof(weight_t))
#define OUTSIZE (MAX_M * MAX_N * sizeof(acc_t))
