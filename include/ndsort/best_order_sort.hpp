#pragma once

#include <functional>

#include "ndsort/detail/flat_fitness.hpp"
#include "ndsort/fronts.hpp"
#include "ndsort/ndsort_export.hpp"
#include "ndsort/traits.hpp"

namespace ndsort {

// Best Order Sort (BOS).
// https://doi.org/10.1145/2908961.2931684
struct NDSORT_EXPORT best_order_sorter {
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
struct sorter_traits<best_order_sorter> {
    static constexpr bool parallel_objectives = false;
    static constexpr bool requires_sorted_input = true;
    static constexpr bool is_exact = true;
};

} // namespace ndsort
