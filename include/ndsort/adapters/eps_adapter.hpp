#pragma once

#include <functional>
#include <utility>

#include "ndsort/concepts.hpp"
#include "ndsort/fronts.hpp"

namespace ndsort {

// Burns in a fixed epsilon tolerance so call sites don't need to pass it explicitly.
// The wrapped sorter is stored by value; if it is stateless the adapter is empty.
template<typename S>
struct eps_adapter {
    S m_sorter{};
    double m_eps{};

    template<typename P, typename Proj = std::identity>
        requires population<P, Proj>
    auto operator()(P&& pop, Proj proj = {}) const -> fronts
    {
        return m_sorter(std::forward<P>(pop), m_eps, proj);
    }
};

template<typename S>
eps_adapter(S, double) -> eps_adapter<S>;

} // namespace ndsort
