#pragma once

#include <functional>

#include "ndsort/detail/flat_fitness.hpp"
#include "ndsort/fronts.hpp"
#include "ndsort/ndsort_export.hpp"
#include "ndsort/traits.hpp"

namespace ndsort {

// Dominance-degree non-dominated sorter.
// Builds a degree matrix D where D[i][j] counts objectives on which i ≤ j,
// then reads dominance directly from D[i][j] == m.
// Front extraction uses the standard NSGA-II dominated-count sweep: O(n²) total,
// not O(n²) per front as in naïve implementations.
struct NDSORT_EXPORT dominance_degree_sorter {
    template <typename P, typename Proj = std::identity>
        requires population<P, Proj>
    auto operator()(P&& pop, double eps = 0.0, Proj proj = {}) const -> fronts
    {
        return detail::sort_with_dedup(
            [this](auto const& ff, double e) { return sort_impl(ff, e); },
            std::forward<P>(pop), eps, proj);
    }

    template <typename P, typename Proj = std::identity>
        requires population<P, Proj>
    auto operator()(P&& pop, double eps, Proj proj, presorted_t) const -> fronts
    {
        return detail::sort_presorted_with_dedup(
            [this](auto const& ff, double e) { return sort_impl(ff, e); },
            std::forward<P>(pop), eps, proj);
    }

private:
    template <typename T>
    auto sort_impl(detail::flat_fitness<T> const&, double eps) const -> fronts;
};

template <>
struct sorter_traits<dominance_degree_sorter> {
    static constexpr bool parallel_objectives = false;
    static constexpr bool requires_sorted_input = false;
    static constexpr bool is_exact = true;
};

} // namespace ndsort
