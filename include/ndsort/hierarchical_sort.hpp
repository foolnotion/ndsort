#pragma once

#include <functional>

#include "ndsort/detail/flat_fitness.hpp"
#include "ndsort/fronts.hpp"
#include "ndsort/ndsort_export.hpp"
#include "ndsort/traits.hpp"

namespace ndsort {

// Hierarchical non-dominated sorter (HSS).
struct NDSORT_EXPORT hierarchical_sorter {
    template<typename P, typename Proj = std::identity>
        requires population<P, Proj>
    auto operator()(P&& pop, double eps = 0.0, Proj proj = {}) const -> fronts
    {
        auto ff        = detail::flatten(std::forward<P>(pop), proj);
        auto const perm = detail::lex_perm(ff);
        auto const sff  = detail::reindex(ff, perm);
        auto result    = sort_impl(sff, eps);
        for (auto& front : result) {
            for (auto& idx : front) { idx = perm[idx]; }
        }
        return result;
    }

    template<typename P, typename Proj = std::identity>
        requires population<P, Proj>
    auto operator()(P&& pop, double eps, Proj proj, presorted_t) const -> fronts
    {
        return sort_impl(detail::flatten(std::forward<P>(pop), proj), eps);
    }

private:
    auto sort_impl(detail::flat_fitness const&, double eps) const -> fronts;
};

} // namespace ndsort
