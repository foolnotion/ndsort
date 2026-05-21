// SPDX-License-Identifier: MIT
// Best Order Sort — https://doi.org/10.1145/2908961.2931684

#include "ndsort/best_order_sort.hpp"

#include <algorithm>
#include <cstddef>
#include <numeric>
#include <ranges>
#include <vector>

namespace ndsort {

auto best_order_sorter::sort_impl(detail::flat_fitness const& ff, double /*eps*/) const -> fronts
{
    auto const n = static_cast<int>(ff.n);
    auto const m = static_cast<int>(ff.m);

    // solution_sets[obj][rank] = list of solution indices in that rank set for objective obj
    std::vector<std::vector<std::vector<int>>> solution_sets(m);

    // comparison_sets[i] = objectives not yet used to rank individual i
    std::vector<std::vector<int>> comparison_sets(n);
    for (auto& cset : comparison_sets) {
        cset.resize(m);
        std::iota(cset.begin(), cset.end(), 0);
    }

    std::vector<bool> is_ranked(n, false);
    std::vector<int> rank(n, 0);

    int sorted_count{0};
    int rank_count{1}; // number of fronts so far

    // sorted_by_objective[j] = indices sorted by objective j
    std::vector<std::vector<int>> sorted_by_objective(m);
    sorted_by_objective[0].resize(n);
    std::iota(sorted_by_objective[0].begin(), sorted_by_objective[0].end(), 0);
    for (auto j = 1; j < m; ++j) {
        sorted_by_objective[j] = sorted_by_objective[j - 1];
        std::stable_sort(sorted_by_objective[j].begin(), sorted_by_objective[j].end(),
                         [&](int a, int b) { return ff.at(static_cast<std::size_t>(j), static_cast<std::size_t>(a))
                                                  < ff.at(static_cast<std::size_t>(j), static_cast<std::size_t>(b)); });
    }

    auto add_to_rank_set = [&](int s, int j) {
        auto r = rank[s];
        auto& ss = solution_sets[j];
        if (r >= static_cast<int>(ss.size())) { ss.resize(static_cast<std::size_t>(r) + 1UZ); }
        ss[r].push_back(s);
    };

    // Check if individual s is dominated by individual t using only t's comparison set.
    auto dominated_by = [&](int s, int t) -> bool {
        // t dominates s if t[k] < s[k] for at least one k in comparison_sets[t],
        // and t[k] <= s[k] for all k in comparison_sets[t]... simplified: none_of a[k] < b[k]
        return std::ranges::none_of(comparison_sets[t], [&](int k) {
            auto const ks = static_cast<std::size_t>(k);
            return ff.at(ks, static_cast<std::size_t>(s)) < ff.at(ks, static_cast<std::size_t>(t));
        });
    };

    auto find_rank = [&](int s, int j) {
        bool done{false};
        for (auto k = 0; k < rank_count; ++k) {
            bool dominated{false};
            if (k >= static_cast<int>(solution_sets[j].size())) {
                solution_sets[j].resize(static_cast<std::size_t>(k) + 1UZ);
            }
            for (auto t : solution_sets[j][k]) {
                if (dominated = dominated_by(s, t); dominated) { break; }
            }
            if (!dominated) {
                rank[s] = k;
                done = true;
                add_to_rank_set(s, j);
                break;
            }
        }
        if (!done) {
            rank[s] = rank_count;
            add_to_rank_set(s, j);
            ++rank_count;
        }
    };

    for (auto i = 0; i < n; ++i) {
        for (auto j = 0; j < m; ++j) {
            auto s = sorted_by_objective[j][i];
            std::erase(comparison_sets[s], j);
            if (is_ranked[s]) {
                add_to_rank_set(s, j);
            } else {
                find_rank(s, j);
                is_ranked[s] = true;
                ++sorted_count;
            }
        }
        if (sorted_count == n) { break; }
    }

    auto rmax = static_cast<std::size_t>(*std::max_element(rank.begin(), rank.end()));
    fronts result(rmax + 1UZ);
    for (auto i = 0; i < n; ++i) {
        result[static_cast<std::size_t>(rank[i])].push_back(static_cast<std::size_t>(i));
    }
    return result;
}

} // namespace ndsort
