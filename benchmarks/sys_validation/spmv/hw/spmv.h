/*
Based on algorithm described here:
http://www.cs.berkeley.edu/~mhoemmen/matrix-seminar/slides/UCB_sparse_tutorial_1.pdf
*/

#include <stdio.h>
#include <stdlib.h>

#include "../defines.h"
#include "support.h"

void spmv(TYPE val[NNZ], int32_t cols[NNZ], int32_t rowDelimiters[N + 1],
          TYPE vec[N], TYPE out[N]);
////////////////////////////////////////////////////////////////////////////////
// Test harness interface code.

struct bench_args_t
{
  TYPE val[NNZ];
  int32_t cols[NNZ];
  int32_t rowDelimiters[N+1];
  TYPE vec[N];
  TYPE out[N];
};
