// SPDX-License-Identifier: MIT

#include <cstdio>
#include <vector>

#include "ndsort/ndsort.hpp"

auto main() -> int
{
    std::vector<std::vector<double>> pop {
        { 0.0, 5.0 },
        { 1.0, 3.0 },
        { 2.0, 1.0 },
        { 3.0, 0.0 },
        { 1.5, 1.5 },
    };

    auto fronts = ndsort::rank_intersect_sorter {}(pop);

    for (std::size_t i = 0; i < fronts.size(); ++i) {
        std::printf("Front %zu:", i);
        for (auto idx : fronts[i]) {
            std::printf(" [%zu]{%.1f, %.1f}", idx, pop[idx][0], pop[idx][1]);
        }
        std::putchar('\n');
    }

    return 0;
}
