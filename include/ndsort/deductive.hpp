#pragma once

#include <functional>

#include "ndsort/detail/flat_fitness.hpp"
#include "ndsort/fronts.hpp"
#include "ndsort/ndsort_export.hpp"
#include "ndsort/traits.hpp"

namespace ndsort {

// Deductive non-dominated sorter.
// Reference O(n²m) implementation; exact. Used as correctness oracle in tests.
struct NDSORT_EXPORT deductive_sorter {
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

} // namespace ndsort
