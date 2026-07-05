#include <cstdio>
#include <cstdlib>
#include <random>

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

    double *X = new double[N];
    double *Y = new double[N];
    double alpha = 0.5;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(1, 2);
    for (int i = 0; i < N; ++i) {
        X[i] = dis(gen);
        Y[i] = dis(gen);
    }

    m5_hypercall(WORK_BEGIN);
    for (int r = 0; r < repeats; ++r) {
        for (int i = 0; i < N; ++i)
            Y[i] = alpha * X[i] + Y[i];
    }
    m5_hypercall(WORK_END);

    double sum = 0;
    for (int i = 0; i < N; ++i)
        sum += Y[i];
    printf("%lf\n", sum);

    delete[] X;
    delete[] Y;
    return 0;
}
