#include <cstdio>
#include <random>

#include <gem5/m5ops.h>

#define WORK_BEGIN 1999
#define WORK_END 2000

int
main()
{
    const int N = 4096;
    int X[N], Y[N], alpha = 2;
    for (int i = 0; i < N; ++i) {
        X[i] = rand();
    }

    // Start of daxpy loop
    // m5_dump_reset_stats(0,0);
    m5_hypercall(WORK_BEGIN);
    for (int i = 0; i < N; ++i) {
        Y[i] = alpha * X[i];
    }
    m5_hypercall(WORK_END);
    // m5_dump_reset_stats(0,0);
    //  End of daxpy loop

    int sum = 0;
    for (int i = 0; i < N; ++i) {
        sum += Y[i];
    }
    printf("%d\n", sum);
    return 0;
}
