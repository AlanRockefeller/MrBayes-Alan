#include "libhmsbeagle/beagle.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <vector>

static void check(int n) { if (n < 0) std::exit(1); }
static double cpu() {
    timespec t;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t);
    return t.tv_sec + t.tv_nsec * 1e-9;
}

int main() {
    for (int n : {209, 394, 1191}) {
        const long flags = BEAGLE_FLAG_PRECISION_SINGLE | BEAGLE_FLAG_PROCESSOR_CPU |
            BEAGLE_FLAG_VECTOR_NONE | BEAGLE_FLAG_SCALING_MANUAL |
            BEAGLE_FLAG_SCALERS_RAW | BEAGLE_FLAG_THREADING_NONE;
        BeagleInstanceDetails info;
        int instance = beagleCreateInstance(2, 5, 2, 4, n, 1, 2, 4, 1,
                                           NULL, 0, flags, flags, &info);
        check(instance);
        std::vector<int> states(n);
        for (int k = 0; k < n; ++k) states[k] = k % 5;
        check(beagleSetTipStates(instance, 0, states.data()));
        for (int k = 0; k < n; ++k) states[k] = (k / 3) % 5;
        check(beagleSetTipStates(instance, 1, states.data()));
        std::vector<double> matrix(64), partials(n * 16);
        for (int k = 0; k < 64; ++k) matrix[k] = (1 + k % 11) / 40.0;
        check(beagleSetTransitionMatrix(instance, 0, matrix.data(), 1.0));
        check(beagleSetTransitionMatrix(instance, 1, matrix.data(), 1.0));
        for (int tiny = 0; tiny < 2; ++tiny) {
            for (int k = 0; k < n * 16; ++k)
                partials[k] = std::ldexp(0.5 + (k % 11) / 32.0, tiny ? -(k % 145) : -(k % 11));
            check(beagleSetPartials(instance, 2, partials.data()));
            check(beagleSetPartials(instance, 3, partials.data()));
            for (int mode = 0; mode < 3; ++mode) {
                if (tiny && mode == 0) continue;
                BeagleOperation op = {4, -1, -1, mode == 2 ? 2 : 0, 0, mode == 0 ? 1 : 3, 1};
                const int repeats = 20000;
                for (int i = 0; i < 20; ++i) check(beagleUpdatePartials(instance, &op, 1, -1));
                double start = cpu();
                for (int i = 0; i < repeats; ++i) check(beagleUpdatePartials(instance, &op, 1, -1));
                double elapsed = cpu() - start;
                std::printf("{\"patterns\":%d,\"kind\":\"%s\",\"tiny\":%d,\"ns_per_update\":%.3f}\n",
                            n, mode == 0 ? "SS" : mode == 1 ? "SP" : "PP", tiny, elapsed * 1e9 / repeats);
                std::fflush(stdout);
            }
        }
        check(beagleFinalizeInstance(instance));
    }
}
