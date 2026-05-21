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
    template<typename P, typename Proj = std::identity>
        requires population<P, Proj>
    auto operator()(P&& pop, double eps = 0.0, Proj proj = {}) const -> fronts
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
