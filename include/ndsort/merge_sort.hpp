// SPDX-License-Identifier: MIT
#pragma once

#include "ndsort/detail/sorter_base.hpp"
#include "ndsort/ndsort_export.hpp"

namespace ndsort {

struct NDSORT_EXPORT merge_sorter : detail::sorter_base<merge_sorter> {
private:
    template <typename T>
    auto sort_impl(detail::flat_fitness<T> const&, double eps) const -> fronts;

    friend struct detail::sorter_base<merge_sorter>;
};

template <>
struct sorter_traits<merge_sorter> {
    static constexpr bool parallel_objectives = false;
    static constexpr bool requires_sorted_input = true;
    static constexpr bool is_exact = true;
};

} // namespace ndsort
