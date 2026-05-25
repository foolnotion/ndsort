// SPDX-License-Identifier: MIT
#pragma once

#include "ndsort/detail/sorter_base.hpp"
#include "ndsort/ndsort_export.hpp"

namespace ndsort {

struct NDSORT_EXPORT efficient_binary_sorter : detail::sorter_base<efficient_binary_sorter> {
private:
    template <typename T>
    auto sort_impl(detail::flat_fitness<T> const&, double eps) const -> fronts;

    friend struct detail::sorter_base<efficient_binary_sorter>;
};

struct NDSORT_EXPORT efficient_sequential_sorter : detail::sorter_base<efficient_sequential_sorter> {
private:
    template <typename T>
    auto sort_impl(detail::flat_fitness<T> const&, double eps) const -> fronts;

    friend struct detail::sorter_base<efficient_sequential_sorter>;
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
