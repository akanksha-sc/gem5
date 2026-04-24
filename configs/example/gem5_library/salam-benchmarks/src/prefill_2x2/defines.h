#ifndef __DEFINES_H__
#define __DEFINES_H__

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK

#define MAX_M 32
#define MAX_N 32
#define MAX_K 32

#define MMU_ROWS 2
#define MMU_COLS 2

#define CFG_SIGNED 0x1u
#define CFG_IS4B 0x2u

typedef int8_t act_t;
typedef int8_t weight_t;
typedef int32_t acc_t;

#endif
