// SPDX-License-Identifier: MIT
#pragma once

#include <functional>

#include "ndsort/concepts.hpp"
#include "ndsort/detail/flat_fitness.hpp"
#include "ndsort/fronts.hpp"
#include "ndsort/traits.hpp"

namespace ndsort::detail {

template <typename Derived>
struct sorter_base {
    template <typename P, typename Proj = std::identity>
        requires population<P, Proj>
    auto operator()(P&& pop, double eps = 0.0, Proj proj = {}) const -> fronts
    {
        return sort_with_dedup(
            [this](auto const& ff, double e) { return self().sort_impl(ff, e); },
            std::forward<P>(pop), eps, proj);
    }

    template <typename P, typename Proj = std::identity>
        requires population<P, Proj>
    auto operator()(P&& pop, double eps, Proj proj, presorted_t) const -> fronts
    {
        return sort_presorted_with_dedup(
            [this](auto const& ff, double e) { return self().sort_impl(ff, e); },
            std::forward<P>(pop), eps, proj);
    }

    template <typename P, typename Proj = std::identity>
        requires population<P, Proj>
    auto operator()(P&& pop, double eps, Proj proj, sorted_unique_t) const -> fronts
    {
        return sort_sorted_unique(
            [this](auto const& ff, double e) { return self().sort_impl(ff, e); },
            std::forward<P>(pop), eps, proj);
    }

private:
    auto self() const -> Derived const& { return static_cast<Derived const&>(*this); }
};

} // namespace ndsort::detail
