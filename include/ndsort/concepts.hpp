// SPDX-License-Identifier: MIT
#pragma once

#include <concepts>
#include <functional>
#include <ranges>
#include <type_traits>

#include "ndsort/fronts.hpp"

namespace ndsort {

template <typename T>
concept supported_fitness_scalar = std::same_as<std::remove_cv_t<T>, float> || std::same_as<std::remove_cv_t<T>, double>;

template <typename F>
concept fitness_vector = std::ranges::random_access_range<F> && std::ranges::sized_range<F>
    && supported_fitness_scalar<std::ranges::range_value_t<F>>;

template <typename Proj, typename Element>
concept fitness_projection = std::invocable<Proj&, Element> && fitness_vector<std::invoke_result_t<Proj&, Element>>;

template <typename P, typename Proj = std::identity>
concept population = std::ranges::random_access_range<P> && std::ranges::sized_range<P>
    && fitness_projection<Proj, std::ranges::range_reference_t<P>>;

template <typename S, typename P, typename Proj = std::identity>
concept nondominated_sorter = population<P, Proj> && requires(S const& s, P const& pop, double eps, Proj proj) {
    { s(pop, eps, proj) } -> std::same_as<fronts>;
    { s(pop, eps) } -> std::same_as<fronts>;
    { s(pop) } -> std::same_as<fronts>;
};

} // namespace ndsort
