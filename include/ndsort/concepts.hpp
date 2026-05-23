#pragma once

#include <concepts>
#include <functional>
#include <ranges>

#include "ndsort/fronts.hpp"

namespace ndsort {

// A fitness vector: random-access range of totally ordered scalars.
template<typename F>
concept fitness_vector =
    std::ranges::random_access_range<F> &&
    std::totally_ordered<std::ranges::range_value_t<F>>;

// A projection that maps an element type to its fitness vector.
template<typename Proj, typename Element>
concept fitness_projection =
    std::invocable<Proj, Element> &&
    fitness_vector<std::invoke_result_t<Proj, Element>>;

// A population: sized random-access range whose elements have a fitness vector
// accessible through Proj.
template<typename P, typename Proj = std::identity>
concept population =
    std::ranges::random_access_range<P> &&
    std::ranges::sized_range<P> &&
    fitness_projection<Proj, std::ranges::range_reference_t<P>>;

// A non-dominated sorter: callable object that accepts a population with an
// optional epsilon tolerance and projection, and returns a fronts partition.
template<typename S, typename P, typename Proj = std::identity>
concept nondominated_sorter =
    population<P, Proj> &&
    requires(S const& s, P const& pop, double eps, Proj proj) {
        { s(pop, eps, proj) } -> std::same_as<fronts>;
        { s(pop, eps)       } -> std::same_as<fronts>;
        { s(pop)            } -> std::same_as<fronts>;
    };

} // namespace ndsort
