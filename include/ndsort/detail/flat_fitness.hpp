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

// Canonical internal representation: fitness values in structure-of-arrays layout.
// data[obj * n + i] = fitness of individual i on objective obj.
// All values are stored as double regardless of the source scalar type.
struct flat_fitness {
    std::vector<double> data;
    std::size_t n{};
    std::size_t m{};

    [[nodiscard]] auto at(std::size_t obj, std::size_t individual) const noexcept -> double
    {
        return data[obj * n + individual];
    }
};

// Build a flat_fitness from any population range and projection.
// The input is traversed exactly once.
template<typename P, typename Proj = std::identity>
    requires population<P, Proj>
auto flatten(P&& pop, Proj proj = {}) -> flat_fitness
{
    auto const n = std::ranges::size(pop);
    auto const& first = std::invoke(proj, *std::ranges::begin(pop));
    auto const m = std::ranges::size(first);

    flat_fitness ff;
    ff.n = n;
    ff.m = m;
    ff.data.resize(n * m);

    for (std::size_t i = 0; i < n; ++i) {
        auto const& fitness = std::invoke(proj, pop[static_cast<std::ptrdiff_t>(i)]);
        for (std::size_t j = 0; j < m; ++j) {
            ff.data[j * n + i] = static_cast<double>(fitness[static_cast<std::ptrdiff_t>(j)]);
        }
    }
    return ff;
}

// Returns a permutation such that individuals are in lexicographic fitness order.
// perm[i] is the original index of the i-th individual in sorted order.
inline auto lex_perm(flat_fitness const& ff) -> std::vector<std::size_t>
{
    auto const n = ff.n;
    auto const m = ff.m;
    std::vector<std::size_t> perm(n);
    std::iota(perm.begin(), perm.end(), std::size_t{0});
    std::stable_sort(perm.begin(), perm.end(), [&](std::size_t a, std::size_t b) {
        for (std::size_t k = 0; k < m; ++k) {
            auto const va = ff.at(k, a);
            auto const vb = ff.at(k, b);
            if (va < vb) { return true; }
            if (va > vb) { return false; }
        }
        return false;
    });
    return perm;
}

// Build a reindexed flat_fitness: result.at(obj, i) == ff.at(obj, perm[i]).
inline auto reindex(flat_fitness const& ff, std::vector<std::size_t> const& perm) -> flat_fitness
{
    auto const n = ff.n;
    auto const m = ff.m;
    flat_fitness result;
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

// Result of eps-aware deduplication of a lex-sorted flat_fitness.
// ff holds only unique representatives; canonical[i] is the index in ff of
// the representative for original lex-sorted index i.
struct dedup_result {
    flat_fitness             ff;
    std::vector<std::size_t> canonical;
};

// Group eps-close solutions in a lex-sorted flat_fitness.
// Two adjacent solutions are considered equal if |a[k] - b[k]| <= eps for all k.
// Each group is represented by its first member (the lex-smallest).
// Takes ff by value so callers can move temporaries in without an extra copy.
inline auto eps_dedup(flat_fitness ff, double eps) -> dedup_result
{
    auto const n = ff.n;
    auto const m = ff.m;

    std::vector<std::size_t> canonical(n);
    std::vector<std::size_t> unique_idx;
    unique_idx.reserve(n);

    for (std::size_t i = 0; i < n; ++i) {
        bool is_dup = !unique_idx.empty();
        for (std::size_t k = 0; k < m && is_dup; ++k) {
            is_dup = std::abs(ff.at(k, i) - ff.at(k, unique_idx.back())) <= eps;
        }
        canonical[i] = is_dup ? unique_idx.size() - 1 : unique_idx.size();
        if (!is_dup) { unique_idx.push_back(i); }
    }

    auto const nu = unique_idx.size();
    if (nu == n) { return {std::move(ff), std::move(canonical)}; }

    flat_fitness uq;
    uq.n = nu;
    uq.m = m;
    uq.data.resize(nu * m);
    for (std::size_t k = 0; k < m; ++k) {
        for (std::size_t j = 0; j < nu; ++j) {
            uq.data[k * nu + j] = ff.at(k, unique_idx[j]);
        }
    }
    return {std::move(uq), std::move(canonical)};
}

// Dispatch on the number of objectives at compile time for m = 1..8,
// falling back to a runtime value (M = 0) for anything larger.
// Usage: dispatch_on_m(ff, [&](auto mc) { return sort_impl<mc.value>(ff, eps); });
template<typename F>
auto dispatch_on_m(flat_fitness const& ff, F&& fn)
{
    switch (ff.m) {
        case 1:  return fn(std::integral_constant<int, 1>{});
        case 2:  return fn(std::integral_constant<int, 2>{});
        case 3:  return fn(std::integral_constant<int, 3>{});
        case 4:  return fn(std::integral_constant<int, 4>{});
        case 5:  return fn(std::integral_constant<int, 5>{});
        case 6:  return fn(std::integral_constant<int, 6>{});
        case 7:  return fn(std::integral_constant<int, 7>{});
        case 8:  return fn(std::integral_constant<int, 8>{});
        default: return fn(std::integral_constant<int, 0>{});
    }
}

// Common preprocessing pipeline: lex sort → eps-dedup → sort_fn → expand ranks.
// sort_fn must be callable as sort_fn(flat_fitness const&, double eps) -> ndsort::fronts.
template<typename SortFn, typename P, typename Proj>
    requires population<P, Proj>
auto sort_with_dedup(SortFn&& sort_fn, P&& pop, double eps, Proj proj) -> ndsort::fronts
{
    if (std::ranges::size(pop) == 0) { return {}; }
    auto ff               = flatten(std::forward<P>(pop), proj);
    auto const perm       = lex_perm(ff);
    auto [uff, canonical] = eps_dedup(reindex(ff, perm), eps);
    auto result           = sort_fn(uff, eps);
    std::vector<std::size_t> urank(uff.n);
    for (std::size_t f = 0; f < result.size(); ++f) {
        for (auto j : result[f]) { urank[j] = f; }
    }
    ndsort::fronts expanded(result.size());
    for (std::size_t i = 0; i < ff.n; ++i) {
        expanded[urank[canonical[i]]].push_back(perm[i]);
    }
    return expanded;
}

// Presorted variant: caller guarantees input is already in lex fitness order.
// Skips lex sort; still deduplicates so identical/eps-close solutions land in the same front.
template<typename SortFn, typename P, typename Proj>
    requires population<P, Proj>
auto sort_presorted_with_dedup(SortFn&& sort_fn, P&& pop, double eps, Proj proj) -> ndsort::fronts
{
    if (std::ranges::size(pop) == 0) { return {}; }
    auto ff               = flatten(std::forward<P>(pop), proj);
    auto [uff, canonical] = eps_dedup(ff, eps);
    auto result           = sort_fn(uff, eps);
    std::vector<std::size_t> urank(uff.n);
    for (std::size_t f = 0; f < result.size(); ++f) {
        for (auto j : result[f]) { urank[j] = f; }
    }
    ndsort::fronts expanded(result.size());
    for (std::size_t i = 0; i < ff.n; ++i) {
        expanded[urank[canonical[i]]].push_back(i);
    }
    return expanded;
}

} // namespace ndsort::detail
