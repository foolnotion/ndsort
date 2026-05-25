#pragma once

#include "ndsort/detail/sorter_base.hpp"
#include "ndsort/ndsort_export.hpp"

namespace ndsort {

struct NDSORT_EXPORT deductive_sorter : detail::sorter_base<deductive_sorter> {
private:
    template <typename T>
    auto sort_impl(detail::flat_fitness<T> const&, double eps) const -> fronts;

    friend struct detail::sorter_base<deductive_sorter>;
};

} // namespace ndsort
