// SPDX-License-Identifier: MIT

#include "ndsort/efficient_sort.hpp"

#include <algorithm>
#include <cstddef>
#include <ranges>
#include <vector>

namespace ndsort {
namespace {

    enum class ens_strategy { binary,
        sequential };

    template <typename T>
    auto weakly_dominates(detail::flat_fitness<T> const& ff, std::size_t j, std::size_t i, double eps) -> bool
    {
        for (std::size_t k = 0; k < ff.m; ++k) {
            if (ff.at(k, j) > ff.at(k, i) + static_cast<T>(eps)) {
                return false;
            }
        }
        return true;
    }

    template <ens_strategy Strategy, typename T>
    auto ens_sort_impl(detail::flat_fitness<T> const& ff, double eps) -> fronts
    {
        auto const n = ff.n;

        auto dominated_by_front = [&](std::vector<std::size_t> const& f, std::size_t i) -> bool {
            return std::ranges::any_of(std::views::reverse(f), [&](std::size_t j) {
                return weakly_dominates(ff, j, i, eps);
            });
        };

        fronts result;
        for (std::size_t i = 0; i < n; ++i) {
            decltype(result)::iterator it;
            if constexpr (Strategy == ens_strategy::binary) {
                it = std::partition_point(result.begin(), result.end(),
                    [&](auto const& f) { return dominated_by_front(f, i); });
            } else {
                it = std::ranges::find_if(result,
                    [&](auto const& f) { return !dominated_by_front(f, i); });
            }
            if (it == result.end()) {
                result.push_back({ i });
            } else {
                it->push_back(i);
            }
        }
        return result;
    }

} // namespace

template <typename T>
auto efficient_binary_sorter::sort_impl(detail::flat_fitness<T> const& ff, double eps) const -> fronts
{
    return ens_sort_impl<ens_strategy::binary>(ff, eps);
}

template <typename T>
auto efficient_sequential_sorter::sort_impl(detail::flat_fitness<T> const& ff, double eps) const -> fronts
{
    return ens_sort_impl<ens_strategy::sequential>(ff, eps);
}

template auto efficient_binary_sorter::sort_impl<float>(detail::flat_fitness<float> const&, double) const -> fronts;
template auto efficient_binary_sorter::sort_impl<double>(detail::flat_fitness<double> const&, double) const -> fronts;
template auto efficient_sequential_sorter::sort_impl<float>(detail::flat_fitness<float> const&, double) const -> fronts;
template auto efficient_sequential_sorter::sort_impl<double>(detail::flat_fitness<double> const&, double) const -> fronts;

} // namespace ndsort
