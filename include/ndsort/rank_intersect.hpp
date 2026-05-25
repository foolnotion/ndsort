// SPDX-License-Identifier: MIT
#pragma once

#include "ndsort/detail/sorter_base.hpp"
#include "ndsort/ndsort_export.hpp"

namespace ndsort {

struct NDSORT_EXPORT rank_intersect_sorter : detail::sorter_base<rank_intersect_sorter> {
private:
    template <typename T>
    auto sort_impl(detail::flat_fitness<T> const&, double eps) const -> fronts;

    friend struct detail::sorter_base<rank_intersect_sorter>;
};

template <>
struct sorter_traits<rank_intersect_sorter> {
    static constexpr bool parallel_objectives = true;
    static constexpr bool requires_sorted_input = true;
    static constexpr bool is_exact = true;
};

} // namespace ndsort
