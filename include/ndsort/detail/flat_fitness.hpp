// SPDX-License-Identifier: MIT
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <numeric>
#include <ranges>
#include <type_traits>
#include <vector>

#include "ndsort/concepts.hpp"

namespace ndsort::detail {

template <typename T>
struct flat_fitness {
    static_assert(std::is_arithmetic_v<T>, "T must be arithmetic");

    std::vector<T> data;
    std::size_t n {};
    std::size_t m {};

    [[nodiscard]] auto at(std::size_t obj, std::size_t individual) const noexcept -> T
    {
        return data[obj * n + individual];
    }
};

template <typename P, typename Proj = std::identity>
    requires population<P, Proj>
auto flatten(P&& pop, Proj proj = {})
{
    using element_t = std::ranges::range_value_t<P>;
    using fitness_t = std::ranges::range_value_t<std::invoke_result_t<Proj, element_t>>;
    using T = std::conditional_t<std::is_floating_point_v<fitness_t>, fitness_t, double>;

    auto const n = std::ranges::size(pop);
    if (n == 0) {
        return flat_fitness<T> {};
    }
    auto const& first = std::invoke(proj, *std::ranges::begin(pop));
    auto const m = std::ranges::size(first);

    flat_fitness<T> ff;
    ff.n = n;
    ff.m = m;
    ff.data.resize(n * m);

    auto pop_it = std::ranges::begin(pop);
    for (std::size_t i = 0; i < n; ++i, ++pop_it) {
        auto const& fitness = std::invoke(proj, *pop_it);
        auto fit_it = std::ranges::begin(fitness);
        for (std::size_t j = 0; j < m; ++j, ++fit_it) {
            ff.data[j * n + i] = static_cast<T>(*fit_it);
        }
    }
    return ff;
}

template <typename T>
auto lex_perm(flat_fitness<T> const& ff) -> std::vector<std::size_t>
{
    auto const n = ff.n;
    auto const m = ff.m;
    std::vector<std::size_t> perm(n);
    std::iota(perm.begin(), perm.end(), std::size_t { 0 });
    std::stable_sort(perm.begin(), perm.end(), [&](std::size_t a, std::size_t b) {
        for (std::size_t k = 0; k < m; ++k) {
            auto const va = ff.at(k, a);
            auto const vb = ff.at(k, b);
            if (va < vb) {
                return true;
            }
            if (va > vb) {
                return false;
            }
        }
        return false;
    });
    return perm;
}

template <typename T>
auto reindex(flat_fitness<T> const& ff, std::vector<std::size_t> const& perm) -> flat_fitness<T>
{
    auto const n = ff.n;
    auto const m = ff.m;
    flat_fitness<T> result;
    result.n = n;
    result.m = m;
    result.data.resize(n * m);
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t k = 0; k < m; ++k) {
            result.data[k * n + i] = ff.data[k * n + perm[i]];
        }
    }
    return result;
}

template <typename T>
struct dedup_result {
    flat_fitness<T> ff;
    std::vector<std::size_t> canonical;
};

template <typename T>
auto eps_dedup(flat_fitness<T> ff, double eps) -> dedup_result<T>
{
    auto const n = ff.n;
    auto const m = ff.m;

    std::vector<std::size_t> canonical(n);
    std::vector<std::size_t> unique_idx;
    unique_idx.reserve(n);

    for (std::size_t i = 0; i < n; ++i) {
        bool is_dup = !unique_idx.empty();
        for (std::size_t k = 0; k < m && is_dup; ++k) {
            auto const diff = ff.at(k, i) - ff.at(k, unique_idx.back());
            is_dup = (diff < 0 ? -diff : diff) <= static_cast<T>(eps);
        }
        canonical[i] = is_dup ? unique_idx.size() - 1 : unique_idx.size();
        if (!is_dup) {
            unique_idx.push_back(i);
        }
    }

    auto const nu = unique_idx.size();
    if (nu == n) {
        return { std::move(ff), std::move(canonical) };
    }

    flat_fitness<T> uq;
    uq.n = nu;
    uq.m = m;
    uq.data.resize(nu * m);
    for (std::size_t k = 0; k < m; ++k) {
        for (std::size_t j = 0; j < nu; ++j) {
            uq.data[k * nu + j] = ff.at(k, unique_idx[j]);
        }
    }
    return { std::move(uq), std::move(canonical) };
}

template <typename T, typename F>
auto dispatch_on_m(flat_fitness<T> const& ff, F&& fn)
{
    switch (ff.m) {
    case 1:
        return fn(std::integral_constant<int, 1> {});
    case 2:
        return fn(std::integral_constant<int, 2> {});
    case 3:
        return fn(std::integral_constant<int, 3> {});
    case 4:
        return fn(std::integral_constant<int, 4> {});
    case 5:
        return fn(std::integral_constant<int, 5> {});
    case 6:
        return fn(std::integral_constant<int, 6> {});
    case 7:
        return fn(std::integral_constant<int, 7> {});
    case 8:
        return fn(std::integral_constant<int, 8> {});
    default:
        return fn(std::integral_constant<int, 0> {});
    }
}

template <typename SortFn, typename P, typename Proj>
    requires population<P, Proj>
auto sort_with_dedup(SortFn&& sort_fn, P&& pop, double eps, Proj proj) -> ndsort::fronts
{
    if (std::ranges::size(pop) == 0) {
        return {};
    }
    auto ff = flatten(std::forward<P>(pop), proj);
    auto const perm = lex_perm(ff);
    auto [uff, canonical] = eps_dedup(reindex(ff, perm), eps);
    auto result = sort_fn(uff, eps);
    std::vector<std::size_t> urank(uff.n);
    for (std::size_t f = 0; f < result.size(); ++f) {
        for (auto j : result[f]) {
            urank[j] = f;
        }
    }
    ndsort::fronts expanded(result.size());
    for (std::size_t i = 0; i < ff.n; ++i) {
        expanded[urank[canonical[i]]].push_back(perm[i]);
    }
    return expanded;
}

// Fastest path: caller guarantees input is lexicographically sorted with no duplicates.
// Skips lex_perm, eps_dedup, and index reconstruction — directly returns sort_fn output.
template <typename SortFn, typename P, typename Proj>
    requires population<P, Proj>
auto sort_sorted_unique(SortFn&& sort_fn, P&& pop, double eps, Proj proj) -> ndsort::fronts
{
    if (std::ranges::size(pop) == 0) {
        return {};
    }
    auto ff = flatten(std::forward<P>(pop), proj);
    return sort_fn(ff, eps);
}

template <typename SortFn, typename P, typename Proj>
    requires population<P, Proj>
auto sort_presorted_with_dedup(SortFn&& sort_fn, P&& pop, double eps, Proj proj) -> ndsort::fronts
{
    if (std::ranges::size(pop) == 0) {
        return {};
    }
    auto ff = flatten(std::forward<P>(pop), proj);
    auto const n = ff.n;
    auto [uff, canonical] = eps_dedup(std::move(ff), eps);
    auto result = sort_fn(uff, eps);
    std::vector<std::size_t> urank(uff.n);
    for (std::size_t f = 0; f < result.size(); ++f) {
        for (auto j : result[f]) {
            urank[j] = f;
        }
    }
    ndsort::fronts expanded(result.size());
    for (std::size_t i = 0; i < n; ++i) {
        expanded[urank[canonical[i]]].push_back(i);
    }
    return expanded;
}

} // namespace ndsort::detail
