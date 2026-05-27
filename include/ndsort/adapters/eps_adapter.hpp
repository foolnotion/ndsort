// SPDX-License-Identifier: MIT
#pragma once

#include <functional>
#include <utility>

#include "ndsort/concepts.hpp"
#include "ndsort/fronts.hpp"
#include "ndsort/traits.hpp"

namespace ndsort {

// Burns in a fixed epsilon tolerance so call sites don't need to pass it explicitly.
// The wrapped sorter is stored by value; if it is stateless the adapter is empty.
// The stored epsilon is always used; any eps argument passed by the caller is ignored.
template <typename S>
struct eps_adapter {
    S sorter_ {};
    double eps_ {};

    template <typename P>
        requires population<P> && std::invocable<S const&, P, double>
    auto operator()(P&& pop) const -> fronts
    {
        return sorter_(std::forward<P>(pop), eps_);
    }

    // eps argument is intentionally ignored; eps_ is always used.
    // These overloads exist solely to satisfy the nondominated_sorter concept.
    template <typename P>
        requires population<P> && std::invocable<S const&, P, double>
    auto operator()(P&& pop, double /*eps — use eps_ instead*/) const -> fronts
    {
        return sorter_(std::forward<P>(pop), eps_);
    }

    template <typename P, typename Proj>
        requires population<P, Proj> && std::invocable<S const&, P, double, Proj>
    auto operator()(P&& pop, double /*eps — use eps_ instead*/, Proj proj) const -> fronts
    {
        return sorter_(std::forward<P>(pop), eps_, proj);
    }

    template <typename P, typename Proj>
        requires population<P, Proj> && std::invocable<S const&, P, double, Proj, presorted_t>
    auto operator()(P&& pop, double /*ignored*/, Proj proj, presorted_t) const -> fronts
    {
        return sorter_(std::forward<P>(pop), eps_, proj, presorted);
    }

    template <typename P, typename Proj>
        requires population<P, Proj> && std::invocable<S const&, P, double, Proj, sorted_unique_t>
    auto operator()(P&& pop, double /*ignored*/, Proj proj, sorted_unique_t) const -> fronts
    {
        return sorter_(std::forward<P>(pop), eps_, proj, sorted_unique);
    }
};

template <typename S>
eps_adapter(S, double) -> eps_adapter<S>;

} // namespace ndsort
