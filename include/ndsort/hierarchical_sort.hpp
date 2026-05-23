#pragma once

#include <functional>

#include "ndsort/detail/flat_fitness.hpp"
#include "ndsort/fronts.hpp"
#include "ndsort/ndsort_export.hpp"
#include "ndsort/traits.hpp"

namespace ndsort {

// Hierarchical non-dominated sorter (HSS).
struct NDSORT_EXPORT hierarchical_sorter {
    template<typename P, typename Proj = std::identity>
        requires population<P, Proj>
    auto operator()(P&& pop, double eps = 0.0, Proj proj = {}) const -> fronts
    {
        return detail::sort_with_dedup(
            [this](detail::flat_fitness const& ff, double e) { return sort_impl(ff, e); },
            std::forward<P>(pop), eps, proj);
    }

    template<typename P, typename Proj = std::identity>
        requires population<P, Proj>
    auto operator()(P&& pop, double eps, Proj proj, presorted_t) const -> fronts
    {
        return detail::sort_presorted_with_dedup(
            [this](detail::flat_fitness const& ff, double e) { return sort_impl(ff, e); },
            std::forward<P>(pop), eps, proj);
    }

private:
    auto sort_impl(detail::flat_fitness const&, double eps) const -> fronts;
};

template<>
struct sorter_traits<hierarchical_sorter> {
    static constexpr bool parallel_objectives   = false;
    static constexpr bool requires_sorted_input = true;
    static constexpr bool is_exact              = true;
};

} // namespace ndsort
