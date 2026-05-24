#pragma once

#include <functional>

#include "ndsort/detail/flat_fitness.hpp"
#include "ndsort/fronts.hpp"
#include "ndsort/ndsort_export.hpp"
#include "ndsort/traits.hpp"

namespace ndsort {

// ENS-BS: Efficient Non-dominated Sorting with Binary Search.
// Requires lexicographically sorted input; the default operator() handles this.
struct NDSORT_EXPORT efficient_binary_sorter {
    template <typename P, typename Proj = std::identity>
        requires population<P, Proj>
    auto operator()(P&& pop, double eps = 0.0, Proj proj = {}) const -> fronts
    {
        return detail::sort_with_dedup(
            [this](auto const& ff, double e) { return sort_impl(ff, e); },
            std::forward<P>(pop), eps, proj);
    }

    // Presorted overload: caller guarantees population is already in lexicographic order.
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

// ENS-SS: Efficient Non-dominated Sorting with Sequential Search.
// Requires lexicographically sorted input; the default operator() handles this.
struct NDSORT_EXPORT efficient_sequential_sorter {
    template <typename P, typename Proj = std::identity>
        requires population<P, Proj>
    auto operator()(P&& pop, double eps = 0.0, Proj proj = {}) const -> fronts
    {
        return detail::sort_with_dedup(
            [this](auto const& ff, double e) { return sort_impl(ff, e); },
            std::forward<P>(pop), eps, proj);
    }

    // Presorted overload: caller guarantees population is already in lexicographic order.
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
struct sorter_traits<efficient_binary_sorter> {
    static constexpr bool parallel_objectives = false;
    static constexpr bool requires_sorted_input = true;
    static constexpr bool is_exact = true;
};

template <>
struct sorter_traits<efficient_sequential_sorter> {
    static constexpr bool parallel_objectives = false;
    static constexpr bool requires_sorted_input = true;
    static constexpr bool is_exact = true;
};

} // namespace ndsort
