// Exercise the public API and write every resulting partial as binary doubles.
// Run once with each library, then cmp the files (no numeric tolerance).
#include "libhmsbeagle/beagle.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <vector>

static void check(int rc) {
    if (rc < 0) { std::fprintf(stderr, "BEAGLE error %d\n", rc); std::exit(1); }
}

int main() {
    std::mt19937 rng(987123);
    const int ns[] = {1, 63, 64, 65, 129, 257, 513};
    const int cats[] = {1, 4, 5, 8};
    for (int n : ns) for (int categories : cats) {
        BeagleInstanceDetails details;
        const long flags = BEAGLE_FLAG_PRECISION_SINGLE | BEAGLE_FLAG_PROCESSOR_CPU |
                BEAGLE_FLAG_VECTOR_NONE | BEAGLE_FLAG_SCALING_MANUAL |
                BEAGLE_FLAG_SCALERS_RAW | BEAGLE_FLAG_THREADING_NONE;
        int instance = beagleCreateInstance(4, 16, 2, 4, n, 1, 2, categories,
                                           3, NULL, 0, flags, flags, &details);
        check(instance);
        std::vector<int> states(n);
        std::vector<double> tip(n * 4), partials((size_t) n * 4 * categories);
        std::vector<double> out(partials.size()), matrix(categories * 16), weights(n, 1.0);
        check(beagleSetPatternWeights(instance, weights.data()));
        const double freqs[] = {0.1, 0.2, 0.3, 0.4};
        check(beagleSetStateFrequencies(instance, 0, freqs));
        std::vector<double> catWeights(categories, 1.0 / categories);
        check(beagleSetCategoryWeights(instance, 0, catWeights.data()));
        std::vector<double> rates(categories);
        for (int l = 0; l < categories; ++l) rates[l] = (l + 0.125) / categories;
        check(beagleSetCategoryRates(instance, rates.data()));
        for (int trial = 0; trial < 30; ++trial) {
            double eigenVectors[16], inverse[16], eigen[4];
            for (double& x : eigenVectors) x = ((int) (rng() % 2000) - 1000) / 1000.0;
            for (double& x : inverse) x = ((int) (rng() % 2000) - 1000) / 1000.0;
            for (int j = 0; j < 4; ++j) eigen[j] = -(double) (rng() % 1000) / 100.0;
            if (trial % 2 == 0) eigen[0] = 0;
            check(beagleSetEigenDecomposition(instance, 0, eigenVectors, inverse, eigen));
            int indices[] = {0, 1};
            double lengths[] = {std::ldexp(0.173, trial - 15), 0.719 * trial};
            check(beagleUpdateTransitionMatrices(instance, 0, indices, NULL, NULL, lengths, 2));
            for (int m = 0; m < 2; ++m) {
                check(beagleGetTransitionMatrix(instance, m, matrix.data()));
                std::fwrite(matrix.data(), sizeof(double), matrix.size(), stdout);
            }
            for (int m = 0; m < 2; ++m) {
                for (double& x : matrix) x = (1 + rng() % 1000) / 4000.0;
                check(beagleSetTransitionMatrix(instance, m, matrix.data(), 1.0));
            }
            for (int t = 0; t < 2; ++t) {
                for (int k = 0; k < n; ++k) states[k] = (k / (t + 1) + trial) % 5;
                check(beagleSetTipStates(instance, t, states.data()));
            }
            for (int t = 2; t < 4; ++t) {
                for (int k = 0; k < n; ++k) for (int j = 0; j < 4; ++j)
                    tip[4*k+j] = ((k + j + t + trial) % 3) != 0;
                check(beagleSetTipPartials(instance, t, tip.data()));
            }
            for (int t = 4; t < 6; ++t) {
                const int groups = trial % 3 == 0 ? n : 1 + trial % 13;
                for (int l = 0; l < categories; ++l) for (int k = 0; k < n; ++k)
                    for (int j = 0; j < 4; ++j) {
                        const int key = (k % groups) * 7 + l * 5 + j + trial + t;
                        partials[l*n*4+4*k+j] = std::ldexp(0.5 + (key % 31) / 64.0,
                                                              -(key * 17 % 151));
                    }
                check(beagleSetPartials(instance, t, partials.data()));
            }
            BeagleOperation ops[] = {
                {6, 0, -1, 4, 0, 5, 1},
                {7, -1, -1, 0, 0, 1, 1},
                {8, -1, -1, 0, 1, 3, 0},
                {9, -1, -1, 8, 0, 7, 1},
                {10, -1, 0, 0, 0, 1, 1},
                {11, -1, 0, 9, 1, 8, 0},
                {12, 1, -1, 10, 1, 5, 0},
                {13, -1, 1, 2, 0, 4, 1},
                {14, -1, -1, 11, 1, 13, 0},
                {15, -1, -1, 3, 0, 1, 1},
            };
            check(beagleUpdatePartials(instance, ops, sizeof(ops) / sizeof(*ops), -1));
            for (int t = 6; t < 16; ++t) {
                check(beagleGetPartials(instance, t, -1, out.data()));
                if (std::fwrite(out.data(), sizeof(double), out.size(), stdout) != out.size()) return 1;
            }
            // Reuse the buffers and maps, with a different operation topology.
            std::swap(ops[1].child1Partials, ops[1].child2Partials);
            check(beagleUpdatePartials(instance, ops, sizeof(ops) / sizeof(*ops), -1));
            check(beagleGetPartials(instance, 14, -1, out.data()));
            std::fwrite(out.data(), sizeof(double), out.size(), stdout);
            // Aliased input/output is conservatively sent to the old kernel.
            BeagleOperation alias = {14, -1, -1, 14, 0, 3, 1};
            check(beagleUpdatePartials(instance, &alias, 1, -1));
            check(beagleGetPartials(instance, 14, -1, out.data()));
            std::fwrite(out.data(), sizeof(double), out.size(), stdout);
        }
        check(beagleFinalizeInstance(instance));
    }
    return 0;
}
