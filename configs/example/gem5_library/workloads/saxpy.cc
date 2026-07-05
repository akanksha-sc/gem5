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

    float *X = new float[N];
    float *Y = new float[N];
    float alpha = 0.5f;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(1, 2);
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

    float sum = 0;
    for (int i = 0; i < N; ++i)
        sum += Y[i];
    printf("%f\n", sum);

    delete[] X;
    delete[] Y;
    return 0;
}
