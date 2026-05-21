#include <algorithm>
#include <cstddef>
#include <random>
#include <ranges>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "ndsort/ndsort.hpp"

namespace {

using population_t = std::vector<std::vector<double>>;

// Generate a random population with n individuals and m objectives.
auto make_population(std::size_t n, std::size_t m, std::uint64_t seed) -> population_t
{
    std::mt19937_64 rng{seed};
    std::uniform_real_distribution<double> dist{0.0, 1.0};
    population_t pop(n, std::vector<double>(m));
    for (auto& ind : pop) {
        for (auto& v : ind) { v = dist(rng); }
    }
    return pop;
}

// Sort a population with a sorter and return normalised fronts (each front sorted).
template<typename S>
auto sorted_fronts(S const& sorter, population_t const& pop, double eps = 0.0) -> ndsort::fronts
{
    auto f = sorter(pop, eps);
    for (auto& front : f) { std::ranges::sort(front); }
    return f;
}

// Lexicographically sorted population (required by ENS sorters).
auto lex_sorted(population_t pop) -> population_t
{
    std::ranges::sort(pop);
    return pop;
}

} // namespace

TEST_CASE("deductive sorter — trivial cases")
{
    ndsort::deductive_sorter sorter;

    SECTION("single individual") {
        population_t pop{{1.0, 2.0}};
        auto f = sorter(pop);
        REQUIRE(f.size() == 1);
        REQUIRE(f[0] == std::vector<std::size_t>{0});
    }

    SECTION("two non-dominating individuals") {
        population_t pop{{0.0, 1.0}, {1.0, 0.0}};
        auto f = sorter(pop);
        REQUIRE(f.size() == 1);
        REQUIRE(f[0].size() == 2);
    }

    SECTION("one dominates the other") {
        population_t pop{{0.0, 0.0}, {1.0, 1.0}};
        auto f = sorter(pop);
        REQUIRE(f.size() == 2);
        REQUIRE(f[0] == std::vector<std::size_t>{0});
        REQUIRE(f[1] == std::vector<std::size_t>{1});
    }
}

TEST_CASE("all sorters agree with deductive — 2 objectives")
{
    auto const [n, seed] = GENERATE(
        std::pair{100UZ, 42UL}, std::pair{500UZ, 1UL}, std::pair{1000UZ, 123UL});

    auto const pop = make_population(n, 2, seed);
    auto const ref = sorted_fronts(ndsort::deductive_sorter{}, pop);

    SECTION("rank_intersect_sorter") {
        REQUIRE(sorted_fronts(ndsort::rank_intersect_sorter{}, pop) == ref);
    }
    SECTION("merge_sorter") {
        REQUIRE(sorted_fronts(ndsort::merge_sorter{}, pop) == ref);
    }
    SECTION("hierarchical_sorter") {
        REQUIRE(sorted_fronts(ndsort::hierarchical_sorter{}, pop) == ref);
    }
    SECTION("best_order_sorter") {
        REQUIRE(sorted_fronts(ndsort::best_order_sorter{}, pop) == ref);
    }
    SECTION("efficient_binary_sorter (pre-sorted)") {
        auto const sorted_pop = lex_sorted(pop);
        auto const ref_sorted = sorted_fronts(ndsort::deductive_sorter{}, sorted_pop);
        REQUIRE(sorted_fronts(ndsort::efficient_binary_sorter{}, sorted_pop) == ref_sorted);
    }
    SECTION("efficient_sequential_sorter (pre-sorted)") {
        auto const sorted_pop = lex_sorted(pop);
        auto const ref_sorted = sorted_fronts(ndsort::deductive_sorter{}, sorted_pop);
        REQUIRE(sorted_fronts(ndsort::efficient_sequential_sorter{}, sorted_pop) == ref_sorted);
    }
}

TEST_CASE("all sorters agree with deductive — multiple objectives")
{
    auto const [n, m, seed] = GENERATE(
        std::tuple{200UZ, 3UZ, 1UL},
        std::tuple{200UZ, 5UZ, 2UL},
        std::tuple{500UZ, 3UZ, 3UL},
        std::tuple{500UZ, 5UZ, 4UL});

    auto const pop = make_population(n, m, seed);
    auto const ref = sorted_fronts(ndsort::deductive_sorter{}, pop);

    SECTION("rank_intersect_sorter") {
        REQUIRE(sorted_fronts(ndsort::rank_intersect_sorter{}, pop) == ref);
    }
    SECTION("merge_sorter") {
        REQUIRE(sorted_fronts(ndsort::merge_sorter{}, pop) == ref);
    }
    SECTION("hierarchical_sorter") {
        REQUIRE(sorted_fronts(ndsort::hierarchical_sorter{}, pop) == ref);
    }
    SECTION("best_order_sorter") {
        REQUIRE(sorted_fronts(ndsort::best_order_sorter{}, pop) == ref);
    }
}

TEST_CASE("projection — sort structs with member fitness")
{
    struct individual {
        std::vector<double> fitness;
        int                 tag{};
    };

    std::vector<individual> pop{
        {{0.0, 1.0}, 1}, {{1.0, 0.0}, 2}, {{0.5, 0.5}, 3}, {{2.0, 2.0}, 4}};

    auto proj = [](individual const& ind) -> std::vector<double> const& { return ind.fitness; };

    auto f = ndsort::rank_intersect_sorter{}(pop, 0.0, proj);
    REQUIRE(f.size() == 2);           // {0,1,2} in front 0, {3} in front 1
    REQUIRE(f[0].size() == 3);
    REQUIRE(f[1].size() == 1);
    REQUIRE(f[1][0] == 3);
}

TEST_CASE("eps_adapter — burns in epsilon")
{
    population_t pop{{0.0, 0.0}, {0.0001, 0.0001}, {1.0, 1.0}};

    // eps=0.01 means "differences <= eps are ignored".
    // {0,0} vs {0.0001,0.0001}: difference is 0.0001 < 0.01 → neither eps-dominates the other.
    // Both eps-dominate {1,1} (difference = ~1.0 >> 0.01).
    // Expected: front 0 = {{0,0},{0.0001,0.0001}}, front 1 = {{1,1}} → 2 fronts.
    auto adapter = ndsort::eps_adapter{ndsort::deductive_sorter{}, 0.01};
    auto f = adapter(pop);
    REQUIRE(f.size() == 2);
}

TEST_CASE("merge_sorter — duplicate individuals land in the same front")
{
    // {0,0} appears three times; all three must end up in front 0.
    // {1,1} is dominated by all copies of {0,0} → front 1.
    population_t pop{{0.0, 0.0}, {0.0, 0.0}, {1.0, 1.0}, {0.0, 0.0}};
    auto f = sorted_fronts(ndsort::merge_sorter{}, pop);
    REQUIRE(f.size() == 2);
    REQUIRE(f[0].size() == 3);  // all three copies of {0,0}
    REQUIRE(f[1].size() == 1);  // {1,1}
    // deductive must agree
    auto ref = sorted_fronts(ndsort::deductive_sorter{}, pop);
    REQUIRE(f == ref);
}

TEST_CASE("nondominated_sorter concept is satisfied")
{
    using pop_t = std::vector<std::vector<double>>;
    static_assert(ndsort::nondominated_sorter<ndsort::deductive_sorter,        pop_t>);
    static_assert(ndsort::nondominated_sorter<ndsort::rank_intersect_sorter,   pop_t>);
    static_assert(ndsort::nondominated_sorter<ndsort::merge_sorter,            pop_t>);
    static_assert(ndsort::nondominated_sorter<ndsort::hierarchical_sorter,     pop_t>);
    static_assert(ndsort::nondominated_sorter<ndsort::best_order_sorter,       pop_t>);
    static_assert(ndsort::nondominated_sorter<ndsort::efficient_binary_sorter, pop_t>);
    SUCCEED();
}
