#pragma once

#include <functional>

#include "ndsort/detail/flat_fitness.hpp"
#include "ndsort/fronts.hpp"
#include "ndsort/ndsort_export.hpp"
#include "ndsort/traits.hpp"

namespace ndsort {

// Rank-intersect non-dominated sorter.
// Uses LSB radix sort, packed triangular bitsets, and SIMD bitset intersection (EVE).
// Fastest in practice for large populations with low-to-mid objective counts.
// EVE and all SIMD machinery are confined to rank_intersect.cpp — no heavy headers leak.
struct NDSORT_EXPORT rank_intersect_sorter {
    // Default: pre-sort population lexicographically, run algorithm, map results back.
    template<typename P, typename Proj = std::identity>
        requires population<P, Proj>
    auto operator()(P&& pop, double eps = 0.0, Proj proj = {}) const -> fronts
    {
        auto ff          = detail::flatten(std::forward<P>(pop), proj);
        auto const perm  = detail::lex_perm(ff);
        auto [uff, canonical] = detail::eps_dedup(detail::reindex(ff, perm), eps);
        auto result      = sort_impl(uff, eps);
        std::vector<std::size_t> urank(uff.n);
        for (std::size_t f = 0; f < result.size(); ++f) {
            for (auto j : result[f]) { urank[j] = f; }
        }
        fronts expanded(result.size());
        for (std::size_t i = 0; i < ff.n; ++i) {
            expanded[urank[canonical[i]]].push_back(perm[i]);
        }
        return expanded;
    }

    // Presorted overload: caller guarantees population is already in lexicographic order.
    template<typename P, typename Proj = std::identity>
        requires population<P, Proj>
    auto operator()(P&& pop, double eps, Proj proj, presorted_t) const -> fronts
    {
        return sort_impl(detail::flatten(std::forward<P>(pop), proj), eps);
    }

private:
    auto sort_impl(detail::flat_fitness const&, double eps) const -> fronts;
};

template<>
struct sorter_traits<rank_intersect_sorter> {
    static constexpr bool parallel_objectives = true;
    static constexpr bool requires_sorted_input = true;
    static constexpr bool is_exact = true;
};

} // namespace ndsort
