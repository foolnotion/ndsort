#pragma once

#include <functional>

#include "ndsort/detail/flat_fitness.hpp"
#include "ndsort/fronts.hpp"
#include "ndsort/ndsort_export.hpp"
#include "ndsort/traits.hpp"

namespace ndsort {

// Rank-ordinal non-dominated sorter.
// Replaces fitness-value comparisons with ordinal rank comparisons across m
// per-objective sort orders, following https://arxiv.org/abs/2203.13654.
// Does not use epsilon dominance internally (eps is handled by the preprocessing
// dedup step); the eps parameter is accepted but ignored in sort_impl.
struct NDSORT_EXPORT rank_ordinal_sorter {
    template <typename P, typename Proj = std::identity>
        requires population<P, Proj>
    auto operator()(P&& pop, double eps = 0.0, Proj proj = {}) const -> fronts
    {
        return detail::sort_with_dedup(
            [this](auto const& ff, double /*e*/) { return sort_impl(ff); },
            std::forward<P>(pop), eps, proj);
    }

    template <typename P, typename Proj = std::identity>
        requires population<P, Proj>
    auto operator()(P&& pop, double eps, Proj proj, presorted_t) const -> fronts
    {
        return detail::sort_presorted_with_dedup(
            [this](auto const& ff, double /*e*/) { return sort_impl(ff); },
            std::forward<P>(pop), eps, proj);
    }

private:
    template <typename T>
    auto sort_impl(detail::flat_fitness<T> const&) const -> fronts;
};

template <>
struct sorter_traits<rank_ordinal_sorter> {
    static constexpr bool parallel_objectives = false;
    static constexpr bool requires_sorted_input = false;
    static constexpr bool is_exact = true;
};

} // namespace ndsort
