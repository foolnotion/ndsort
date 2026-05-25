#pragma once

#include "ndsort/detail/sorter_base.hpp"
#include "ndsort/ndsort_export.hpp"

namespace ndsort {

struct NDSORT_EXPORT dominance_degree_sorter : detail::sorter_base<dominance_degree_sorter> {
private:
    template <typename T>
    auto sort_impl(detail::flat_fitness<T> const&, double eps) const -> fronts;

    friend struct detail::sorter_base<dominance_degree_sorter>;
};

template <>
struct sorter_traits<dominance_degree_sorter> {
    static constexpr bool parallel_objectives = false;
    static constexpr bool requires_sorted_input = false;
    static constexpr bool is_exact = true;
};

} // namespace ndsort
