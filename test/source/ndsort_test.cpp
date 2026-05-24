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
    std::mt19937_64 rng { seed };
    std::uniform_real_distribution<double> dist { 0.0, 1.0 };
    population_t pop(n, std::vector<double>(m));
    for (auto& ind : pop) {
        for (auto& v : ind) {
            v = dist(rng);
        }
    }
    return pop;
}

// Sort a population with a sorter and return normalised fronts (each front sorted).
template <typename S>
auto sorted_fronts(S const& sorter, population_t const& pop, double eps = 0.0) -> ndsort::fronts
{
    auto f = sorter(pop, eps);
    for (auto& front : f) {
        std::ranges::sort(front);
    }
    return f;
}

} // namespace

TEST_CASE("deductive sorter — trivial cases")
{
    ndsort::deductive_sorter sorter;

    SECTION("single individual")
    {
        population_t pop { { 1.0, 2.0 } };
        auto f = sorter(pop);
        REQUIRE(f.size() == 1);
        REQUIRE(f[0] == std::vector<std::size_t> { 0 });
    }

    SECTION("two non-dominating individuals")
    {
        population_t pop { { 0.0, 1.0 }, { 1.0, 0.0 } };
        auto f = sorter(pop);
        REQUIRE(f.size() == 1);
        REQUIRE(f[0].size() == 2);
    }

    SECTION("one dominates the other")
    {
        population_t pop { { 0.0, 0.0 }, { 1.0, 1.0 } };
        auto f = sorter(pop);
        REQUIRE(f.size() == 2);
        REQUIRE(f[0] == std::vector<std::size_t> { 0 });
        REQUIRE(f[1] == std::vector<std::size_t> { 1 });
    }
}

TEST_CASE("all sorters agree with deductive — 2 objectives")
{
    auto const [n, seed] = GENERATE(
        std::pair { 100UZ, 42UL }, std::pair { 500UZ, 1UL }, std::pair { 1000UZ, 123UL });

    auto const pop = make_population(n, 2, seed);
    auto const ref = sorted_fronts(ndsort::deductive_sorter {}, pop);

    SECTION("rank_intersect_sorter")
    {
        REQUIRE(sorted_fronts(ndsort::rank_intersect_sorter {}, pop) == ref);
    }
    SECTION("merge_sorter")
    {
        REQUIRE(sorted_fronts(ndsort::merge_sorter {}, pop) == ref);
    }
    SECTION("hierarchical_sorter")
    {
        REQUIRE(sorted_fronts(ndsort::hierarchical_sorter {}, pop) == ref);
    }
    SECTION("best_order_sorter")
    {
        REQUIRE(sorted_fronts(ndsort::best_order_sorter {}, pop) == ref);
    }
    SECTION("efficient_binary_sorter")
    {
        REQUIRE(sorted_fronts(ndsort::efficient_binary_sorter {}, pop) == ref);
    }
    SECTION("efficient_sequential_sorter")
    {
        REQUIRE(sorted_fronts(ndsort::efficient_sequential_sorter {}, pop) == ref);
    }
}

TEST_CASE("all sorters agree with deductive — multiple objectives")
{
    auto const [n, m, seed] = GENERATE(
        std::tuple { 200UZ, 3UZ, 1UL },
        std::tuple { 200UZ, 5UZ, 2UL },
        std::tuple { 500UZ, 3UZ, 3UL },
        std::tuple { 500UZ, 5UZ, 4UL });

    auto const pop = make_population(n, m, seed);
    auto const ref = sorted_fronts(ndsort::deductive_sorter {}, pop);

    SECTION("rank_intersect_sorter")
    {
        REQUIRE(sorted_fronts(ndsort::rank_intersect_sorter {}, pop) == ref);
    }
    SECTION("merge_sorter")
    {
        REQUIRE(sorted_fronts(ndsort::merge_sorter {}, pop) == ref);
    }
    SECTION("hierarchical_sorter")
    {
        REQUIRE(sorted_fronts(ndsort::hierarchical_sorter {}, pop) == ref);
    }
    SECTION("best_order_sorter")
    {
        REQUIRE(sorted_fronts(ndsort::best_order_sorter {}, pop) == ref);
    }
    SECTION("efficient_binary_sorter")
    {
        REQUIRE(sorted_fronts(ndsort::efficient_binary_sorter {}, pop) == ref);
    }
    SECTION("efficient_sequential_sorter")
    {
        REQUIRE(sorted_fronts(ndsort::efficient_sequential_sorter {}, pop) == ref);
    }
    SECTION("dominance_degree_sorter")
    {
        REQUIRE(sorted_fronts(ndsort::dominance_degree_sorter {}, pop) == ref);
    }
    SECTION("rank_ordinal_sorter")
    {
        REQUIRE(sorted_fronts(ndsort::rank_ordinal_sorter {}, pop) == ref);
    }
}

TEST_CASE("all sorters agree — eps-dedup places near-duplicates in same front")
{
    // {0,0} and {0.005,0.005} differ by 0.005 < eps=0.01 on both objectives.
    // Both are clearly better than {1,1} (diff ~1.0 >> eps).
    // All sorters must agree: 2 fronts, near-dups together in front 0.
    population_t pop { { 0.0, 0.0 }, { 0.005, 0.005 }, { 1.0, 1.0 } };
    constexpr double eps = 0.01;
    auto const ref = sorted_fronts(ndsort::deductive_sorter {}, pop, eps);
    REQUIRE(ref.size() == 2);
    REQUIRE(ref[0].size() == 2);

    SECTION("rank_intersect_sorter")
    {
        REQUIRE(sorted_fronts(ndsort::rank_intersect_sorter {}, pop, eps) == ref);
    }
    SECTION("merge_sorter")
    {
        REQUIRE(sorted_fronts(ndsort::merge_sorter {}, pop, eps) == ref);
    }
    SECTION("hierarchical_sorter")
    {
        REQUIRE(sorted_fronts(ndsort::hierarchical_sorter {}, pop, eps) == ref);
    }
    SECTION("best_order_sorter")
    {
        REQUIRE(sorted_fronts(ndsort::best_order_sorter {}, pop, eps) == ref);
    }
    SECTION("efficient_binary_sorter")
    {
        REQUIRE(sorted_fronts(ndsort::efficient_binary_sorter {}, pop, eps) == ref);
    }
    SECTION("efficient_sequential_sorter")
    {
        REQUIRE(sorted_fronts(ndsort::efficient_sequential_sorter {}, pop, eps) == ref);
    }
    SECTION("dominance_degree_sorter")
    {
        REQUIRE(sorted_fronts(ndsort::dominance_degree_sorter {}, pop, eps) == ref);
    }
    SECTION("rank_ordinal_sorter")
    {
        REQUIRE(sorted_fronts(ndsort::rank_ordinal_sorter {}, pop, eps) == ref);
    }
}

TEST_CASE("projection — sort structs with member fitness")
{
    struct individual {
        std::vector<double> fitness;
        int tag {};
    };

    std::vector<individual> pop {
        { { 0.0, 1.0 }, 1 }, { { 1.0, 0.0 }, 2 }, { { 0.5, 0.5 }, 3 }, { { 2.0, 2.0 }, 4 }
    };

    auto proj = [](individual const& ind) -> std::vector<double> const& { return ind.fitness; };

    auto f = ndsort::rank_intersect_sorter {}(pop, 0.0, proj);
    REQUIRE(f.size() == 2); // {0,1,2} in front 0, {3} in front 1
    REQUIRE(f[0].size() == 3);
    REQUIRE(f[1].size() == 1);
    REQUIRE(f[1][0] == 3);
}

TEST_CASE("eps_adapter — burns in epsilon")
{
    population_t pop { { 0.0, 0.0 }, { 0.0001, 0.0001 }, { 1.0, 1.0 } };

    // eps=0.01 means "differences <= eps are ignored".
    // {0,0} vs {0.0001,0.0001}: difference is 0.0001 < 0.01 → neither eps-dominates the other.
    // Both eps-dominate {1,1} (difference = ~1.0 >> 0.01).
    // Expected: front 0 = {{0,0},{0.0001,0.0001}}, front 1 = {{1,1}} → 2 fronts.
    auto adapter = ndsort::eps_adapter { ndsort::deductive_sorter {}, 0.01 };
    auto f = adapter(pop);
    REQUIRE(f.size() == 2);
}

TEST_CASE("nondominated_sorter concept is satisfied")
{
    using pop_t = std::vector<std::vector<double>>;
    static_assert(ndsort::nondominated_sorter<ndsort::deductive_sorter, pop_t>);
    static_assert(ndsort::nondominated_sorter<ndsort::rank_intersect_sorter, pop_t>);
    static_assert(ndsort::nondominated_sorter<ndsort::merge_sorter, pop_t>);
    static_assert(ndsort::nondominated_sorter<ndsort::hierarchical_sorter, pop_t>);
    static_assert(ndsort::nondominated_sorter<ndsort::best_order_sorter, pop_t>);
    static_assert(ndsort::nondominated_sorter<ndsort::efficient_binary_sorter, pop_t>);
    static_assert(ndsort::nondominated_sorter<ndsort::efficient_sequential_sorter, pop_t>);
    static_assert(ndsort::nondominated_sorter<ndsort::dominance_degree_sorter, pop_t>);
    static_assert(ndsort::nondominated_sorter<ndsort::rank_ordinal_sorter, pop_t>);
    static_assert(ndsort::nondominated_sorter<ndsort::eps_adapter<ndsort::deductive_sorter>, pop_t>);
    SUCCEED();
}

TEST_CASE("edge cases — empty, single individual, single objective")
{
    ndsort::deductive_sorter sorter;

    SECTION("empty population")
    {
        population_t pop;
        auto f = sorter(pop);
        REQUIRE(f.empty());
    }

    SECTION("single-objective")
    {
        population_t pop { { 3.0 }, { 1.0 }, { 2.0 } };
        auto const ref = sorted_fronts(ndsort::deductive_sorter {}, pop);
        REQUIRE(ref.size() == 3);
        for (auto const& front : ref) {
            REQUIRE(front.size() == 1);
        }

        SECTION("rank_intersect_sorter") { REQUIRE(sorted_fronts(ndsort::rank_intersect_sorter {}, pop) == ref); }
        SECTION("merge_sorter") { REQUIRE(sorted_fronts(ndsort::merge_sorter {}, pop) == ref); }
        SECTION("hierarchical_sorter") { REQUIRE(sorted_fronts(ndsort::hierarchical_sorter {}, pop) == ref); }
        SECTION("best_order_sorter") { REQUIRE(sorted_fronts(ndsort::best_order_sorter {}, pop) == ref); }
        SECTION("efficient_binary_sorter") { REQUIRE(sorted_fronts(ndsort::efficient_binary_sorter {}, pop) == ref); }
        SECTION("efficient_sequential_sorter") { REQUIRE(sorted_fronts(ndsort::efficient_sequential_sorter {}, pop) == ref); }
        SECTION("dominance_degree_sorter") { REQUIRE(sorted_fronts(ndsort::dominance_degree_sorter {}, pop) == ref); }
        SECTION("rank_ordinal_sorter") { REQUIRE(sorted_fronts(ndsort::rank_ordinal_sorter {}, pop) == ref); }
    }
}

TEST_CASE("fronts partition — every individual appears exactly once")
{
    auto const [n, m, seed] = GENERATE(
        std::tuple { 100UZ, 2UZ, 7UL },
        std::tuple { 200UZ, 5UZ, 8UL },
        std::tuple { 50UZ, 8UZ, 9UL });

    auto const pop = make_population(n, m, seed);
    auto const f = ndsort::rank_intersect_sorter {}(pop);

    std::vector<bool> seen(n, false);
    for (auto const& front : f) {
        for (auto idx : front) {
            REQUIRE_FALSE(seen[idx]);
            seen[idx] = true;
        }
    }
    REQUIRE(std::ranges::all_of(seen, [](bool b) { return b; }));
}

TEST_CASE("all sorters agree — large m (dispatch fallback path)")
{
    // m=9 exceeds dispatch_on_m's compile-time cases, exercising the runtime fallback.
    auto const pop = make_population(100, 9, 42);
    auto const ref = sorted_fronts(ndsort::deductive_sorter {}, pop);

    SECTION("rank_intersect_sorter")
    {
        REQUIRE(sorted_fronts(ndsort::rank_intersect_sorter {}, pop) == ref);
    }
    SECTION("hierarchical_sorter")
    {
        REQUIRE(sorted_fronts(ndsort::hierarchical_sorter {}, pop) == ref);
    }
    SECTION("best_order_sorter")
    {
        REQUIRE(sorted_fronts(ndsort::best_order_sorter {}, pop) == ref);
    }
    SECTION("efficient_binary_sorter")
    {
        REQUIRE(sorted_fronts(ndsort::efficient_binary_sorter {}, pop) == ref);
    }
    SECTION("efficient_sequential_sorter")
    {
        REQUIRE(sorted_fronts(ndsort::efficient_sequential_sorter {}, pop) == ref);
    }
}

TEST_CASE("presorted overload produces same result as default overload")
{
    auto pop = make_population(200, 3, 55);
    // Sort the population lexicographically to satisfy the presorted contract.
    std::ranges::sort(pop);

    auto const ref = sorted_fronts(ndsort::deductive_sorter {}, pop);

    auto presorted_fronts = [](auto const& sorter, population_t const& p) {
        auto f = sorter(p, 0.0, std::identity {}, ndsort::presorted);
        for (auto& front : f) {
            std::ranges::sort(front);
        }
        return f;
    };

    SECTION("rank_intersect_sorter")
    {
        REQUIRE(presorted_fronts(ndsort::rank_intersect_sorter {}, pop) == ref);
    }
    SECTION("merge_sorter")
    {
        REQUIRE(presorted_fronts(ndsort::merge_sorter {}, pop) == ref);
    }
    SECTION("hierarchical_sorter")
    {
        REQUIRE(presorted_fronts(ndsort::hierarchical_sorter {}, pop) == ref);
    }
    SECTION("best_order_sorter")
    {
        REQUIRE(presorted_fronts(ndsort::best_order_sorter {}, pop) == ref);
    }
    SECTION("efficient_binary_sorter")
    {
        REQUIRE(presorted_fronts(ndsort::efficient_binary_sorter {}, pop) == ref);
    }
    SECTION("efficient_sequential_sorter")
    {
        REQUIRE(presorted_fronts(ndsort::efficient_sequential_sorter {}, pop) == ref);
    }
    SECTION("deductive_sorter")
    {
        REQUIRE(presorted_fronts(ndsort::deductive_sorter {}, pop) == ref);
    }
}

TEST_CASE("presorted + duplicates — identical solutions go in same front")
{
    // Input already in lex order; contains exact duplicates.
    // Presorted overloads must dedup and not push duplicates to later fronts.
    population_t pop { { 0.0, 0.0 }, { 0.0, 0.0 }, { 1.0, 1.0 } };

    auto check = [&pop](auto const& sorter) {
        auto f = sorter(pop, 0.0, std::identity {}, ndsort::presorted);
        REQUIRE(f.size() == 2);
        // Both duplicates must be in the same (first) front.
        REQUIRE(f[0].size() == 2);
        REQUIRE(f[1].size() == 1);
    };

    SECTION("efficient_binary_sorter") { check(ndsort::efficient_binary_sorter {}); }
    SECTION("efficient_sequential_sorter") { check(ndsort::efficient_sequential_sorter {}); }
    SECTION("hierarchical_sorter") { check(ndsort::hierarchical_sorter {}); }
    SECTION("merge_sorter") { check(ndsort::merge_sorter {}); }
    SECTION("best_order_sorter") { check(ndsort::best_order_sorter {}); }
    SECTION("deductive_sorter") { check(ndsort::deductive_sorter {}); }
}

TEST_CASE("best_order_sort — eps on 3 objectives")
{
    // Two solutions that are eps-close on all 3 objectives should end up in the same front.
    population_t pop {
        { 0.0, 0.0, 0.0 },
        { 0.005, 0.005, 0.005 },
        { 1.0, 1.0, 1.0 }
    };
    constexpr double eps = 0.01;

    auto const ref = sorted_fronts(ndsort::deductive_sorter {}, pop, eps);
    REQUIRE(ref.size() == 2);
    REQUIRE(ref[0].size() == 2);

    REQUIRE(sorted_fronts(ndsort::best_order_sorter {}, pop, eps) == ref);
}
