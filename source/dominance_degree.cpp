// SPDX-License-Identifier: MIT

#include "ndsort/dominance_degree.hpp"

#include <algorithm>
#include <cstddef>
#include <numeric>
#include <vector>

namespace ndsort {
namespace {

    template <typename T>
    auto build_degree_matrix(detail::flat_fitness<T> const& ff) -> std::vector<int>
    {
        auto const n = ff.n;
        auto const m = ff.m;

        // D[i*n + j]: number of objectives on which individual i ≤ individual j.
        // Accumulated directly across all objectives — no per-objective matrix allocation.
        std::vector<int> D(n * n, 0);

        std::vector<std::size_t> idx(n);
        for (std::size_t k = 0; k < m; ++k) {
            std::iota(idx.begin(), idx.end(), std::size_t { 0 });
            std::stable_sort(idx.begin(), idx.end(), [&](std::size_t a, std::size_t b) {
                return ff.at(k, a) < ff.at(k, b);
            });

            // Walk through the sorted order; within each group of equal values every member
            // is ≤ every individual from the group start onward (symmetric within the group).
            std::size_t g = 0; // start of current equal-value group
            for (std::size_t i = 0; i < n; ++i) {
                if (i > 0 && ff.at(k, idx[i]) != ff.at(k, idx[i - 1])) {
                    g = i;
                }
                // idx[i] ≤ idx[j] for all j in [g, n):
                //   - j in [g, i): symmetric (same value) — already incremented from j's own pass
                //   - j in [i, n): idx[i] ≤ idx[j] strictly or same-group
                // We only write D[idx[i]][idx[j]] for j >= i to avoid double-counting the
                // strict direction; the within-group symmetric direction is handled by
                // the j < i range when those j's were processed as the outer i.
                for (std::size_t j = i; j < n; ++j) {
                    ++D[idx[i] * n + idx[j]];
                }
                // For j in [g, i) (same group, already processed as outer i):
                // D[idx[i]][idx[j]] must also be incremented because idx[i] ≤ idx[j]
                // (same value). But when j was the outer index it wrote D[idx[j]][idx[i]],
                // not D[idx[i]][idx[j]]. So we need to add those here.
                for (std::size_t j = g; j < i; ++j) {
                    ++D[idx[i] * n + idx[j]];
                }
            }
        }
        return D;
    }

} // namespace

template <typename T>
auto dominance_degree_sorter::sort_impl(detail::flat_fitness<T> const& ff, double /*eps*/) const -> fronts
{
    auto const n = ff.n;
    auto const mi = static_cast<int>(ff.m);

    auto D = build_degree_matrix(ff);

    // dominated_count[j]: number of i that strictly dominate j (D[i][j] == m, i != j).
    // dominates[i]:       set of individuals dominated by i.
    // After eps_dedup, D[i][j] == m && D[j][i] == m with i != j cannot occur
    // (it would mean identical fitness values, which dedup removes).
    std::vector<int> dominated_count(n, 0);
    std::vector<std::vector<std::size_t>> dominates(n);

    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = 0; j < n; ++j) {
            if (i != j && D[i * n + j] == mi) {
                ++dominated_count[j];
                dominates[i].push_back(j);
            }
        }
    }

    // Standard NSGA-II dominated-count sweep: O(n²) total, not O(n²) per front.
    fronts result;
    std::vector<std::size_t> current;
    current.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        if (dominated_count[i] == 0) {
            current.push_back(i);
        }
    }
    while (!current.empty()) {
        std::vector<std::size_t> next;
        for (auto i : current) {
            for (auto j : dominates[i]) {
                if (--dominated_count[j] == 0) {
                    next.push_back(j);
                }
            }
        }
        result.push_back(std::move(current));
        current = std::move(next);
    }
    return result;
}

template NDSORT_EXPORT auto dominance_degree_sorter::sort_impl<float>(detail::flat_fitness<float> const&, double) const -> fronts;
template NDSORT_EXPORT auto dominance_degree_sorter::sort_impl<double>(detail::flat_fitness<double> const&, double) const -> fronts;

} // namespace ndsort
