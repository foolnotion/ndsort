#pragma once

#include "ndsort/detail/sorter_base.hpp"
#include "ndsort/ndsort_export.hpp"

namespace ndsort {

struct NDSORT_EXPORT rank_ordinal_sorter : detail::sorter_base<rank_ordinal_sorter> {
private:
    template <typename T>
    auto sort_impl(detail::flat_fitness<T> const&, double eps) const -> fronts;

    friend struct detail::sorter_base<rank_ordinal_sorter>;
};

template <>
struct sorter_traits<rank_ordinal_sorter> {
    static constexpr bool parallel_objectives = false;
    static constexpr bool requires_sorted_input = false;
    static constexpr bool is_exact = true;
};

} // namespace ndsort
