#include <cstdio>
#include <cstdlib>

#include <gem5/m5ops.h>

#define WORK_BEGIN 1999
#define WORK_END 2000

int
main(int argc, char *argv[])
{
    int N = 4096;
    int repeats = 1;
    if (argc >= 2)
        N = std::atoi(argv[1]);
    if (argc >= 3)
        repeats = std::atoi(argv[2]);

    int *X = new int[N];
    int *Y = new int[N];
    int alpha = 2;
    for (int i = 0; i < N; ++i) {
        X[i] = rand();
        Y[i] = rand();
    }

    m5_hypercall(WORK_BEGIN);
    for (int r = 0; r < repeats; ++r) {
        for (int i = 0; i < N; ++i)
            Y[i] = alpha * X[i] + Y[i];
    }
    m5_hypercall(WORK_END);

    int sum = 0;
    for (int i = 0; i < N; ++i)
        sum += Y[i];
    printf("%d\n", sum);

    delete[] X;
    delete[] Y;
    return 0;
}
