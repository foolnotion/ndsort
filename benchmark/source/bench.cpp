#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <functional>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#define ANKERL_NANOBENCH_IMPLEMENT
#include <nanobench.h>

#include "ndsort/ndsort.hpp"

namespace {

using pop_t = std::vector<std::vector<float>>;

// ---- CSV loader -------------------------------------------------------------

auto load_csv(std::filesystem::path const& path) -> pop_t
{
    std::ifstream in { path };
    if (!in) {
        throw std::runtime_error { "cannot open " + path.string() };
    }

    pop_t pop;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) {
            continue;
        }
        std::vector<float> row;
        std::istringstream ss { line };
        std::string cell;
        while (std::getline(ss, cell, ',')) {
            row.push_back(std::stof(cell));
        }
        if (!row.empty()) {
            pop.push_back(std::move(row));
        }
    }
    return pop;
}

// ---- Synthetic data generators ----------------------------------------------

// Uniform random in [0, 1]^m.
auto make_random(std::size_t n, std::size_t m, std::uint64_t seed = 42) -> pop_t
{
    std::mt19937_64 rng { seed };
    std::uniform_real_distribution<float> dist { 0.0F, 1.0F };
    pop_t pop(n, std::vector<float>(m));
    for (auto& ind : pop) {
        for (auto& v : ind) {
            v = dist(rng);
        }
    }
    return pop;
}

// Points on (m-1)-simplex via exponential sampling: models a converged
// linear Pareto front (DTLZ1-style) where all solutions are non-dominated.
auto make_linear_front(std::size_t n, std::size_t m, std::uint64_t seed = 42) -> pop_t
{
    std::mt19937_64 rng { seed };
    std::exponential_distribution<float> exp { 1.0F };
    pop_t pop(n, std::vector<float>(m));
    for (auto& ind : pop) {
        float sum = 0.0F;
        for (auto& v : ind) {
            v = exp(rng);
            sum += v;
        }
        for (auto& v : ind) {
            v /= sum;
        }
    }
    return pop;
}

// Points on positive unit sphere: models a converged spherical Pareto front
// (DTLZ2-style).
auto make_sphere_front(std::size_t n, std::size_t m, std::uint64_t seed = 42) -> pop_t
{
    std::mt19937_64 rng { seed };
    std::normal_distribution<float> normal { 0.0F, 1.0F };
    pop_t pop(n, std::vector<float>(m));
    for (auto& ind : pop) {
        float sum_sq = 0.0F;
        for (auto& v : ind) {
            v = std::abs(normal(rng));
            sum_sq += v * v;
        }
        float inv = 1.0F / std::sqrt(sum_sq);
        for (auto& v : ind) {
            v *= inv;
        }
    }
    return pop;
}

// Striated: individual i has all objectives equal to i, so individual 0
// strictly dominates all others — worst case for algorithms with O(n * |F0|)
// inner loops (Buzdalov & Shalyto 2014).
auto make_striated(std::size_t n, std::size_t m) -> pop_t
{
    pop_t pop(n, std::vector<float>(m));
    for (std::size_t i = 0; i < n; ++i) {
        for (auto& v : pop[i]) {
            v = static_cast<float>(i);
        }
    }
    return pop;
}

// ---- Sorter registry --------------------------------------------------------

struct sorter_entry {
    std::string name;
    std::function<ndsort::fronts(pop_t const&)> fn;
};

auto all_sorters() -> std::vector<sorter_entry>
{
    return {
        { "deductive", [](pop_t const& p) { return ndsort::deductive_sorter {}(p); } },
        { "rank_intersect", [](pop_t const& p) { return ndsort::rank_intersect_sorter {}(p); } },
        { "merge", [](pop_t const& p) { return ndsort::merge_sorter {}(p); } },
        { "hierarchical", [](pop_t const& p) { return ndsort::hierarchical_sorter {}(p); } },
        { "best_order", [](pop_t const& p) { return ndsort::best_order_sorter {}(p); } },
        { "dominance_degree", [](pop_t const& p) { return ndsort::dominance_degree_sorter {}(p); } },
        { "rank_ordinal", [](pop_t const& p) { return ndsort::rank_ordinal_sorter {}(p); } },
        { "efficient_binary", [](pop_t const& p) { return ndsort::efficient_binary_sorter {}(p); } },
        { "efficient_sequential", [](pop_t const& p) { return ndsort::efficient_sequential_sorter {}(p); } },
    };
}

// ---- Benchmark helpers ------------------------------------------------------

void bench_population(
    ankerl::nanobench::Bench& bench,
    std::string const& label,
    pop_t const& pop)
{
    for (auto const& s : all_sorters()) {
        bench.run(label + " " + s.name, [&] {
            auto f = s.fn(pop);
            ankerl::nanobench::doNotOptimizeAway(f);
        });
    }
}

void bench_synthetic(ankerl::nanobench::Bench& bench)
{
    constexpr std::array<std::pair<std::size_t, std::size_t>, 9> configs { {
        { 100, 2 },
        { 500, 2 },
        { 1000, 2 },
        { 100, 5 },
        { 500, 5 },
        { 1000, 5 },
        { 100, 10 },
        { 500, 10 },
        { 1000, 10 },
    } };

    for (auto [n, m] : configs) {
        auto tag = " n=" + std::to_string(n) + " m=" + std::to_string(m) + " ";
        bench_population(bench, "random" + tag, make_random(n, m));
        bench_population(bench, "linear_front" + tag, make_linear_front(n, m));
        bench_population(bench, "sphere_front" + tag, make_sphere_front(n, m));
        bench_population(bench, "striated" + tag, make_striated(n, m));
    }
}

// Load every *.csv from data_dir and benchmark it.
// The filename encodes the scenario, e.g. "dtlz2_n500_m5.csv".
void bench_from_csv(ankerl::nanobench::Bench& bench, std::filesystem::path const& data_dir)
{
    if (!std::filesystem::exists(data_dir)) {
        return; // data dir not present — skip silently
    }

    std::vector<std::filesystem::path> files;
    for (auto const& entry : std::filesystem::directory_iterator { data_dir }) {
        if (entry.path().extension() == ".csv") {
            files.push_back(entry.path());
        }
    }
    std::ranges::sort(files);

    for (auto const& path : files) {
        auto label = "csv/" + path.stem().string() + " ";
        auto pop = load_csv(path);
        bench_population(bench, label, pop);
    }
}

} // namespace

int main(int argc, char** argv)
{
    std::filesystem::path data_dir;
    bool csv_mode = false;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg { argv[i] };
        if (arg == "--csv") {
            csv_mode = true;
        } else {
            data_dir = argv[i];
        }
    }
    if (data_dir.empty()) {
        data_dir = NDSORT_DATA_DIR;
    }

    ankerl::nanobench::Bench bench;
    bench.title("ndsort benchmarks")
        .unit("sort")
        .warmup(3)
        .minEpochIterations(5);

    // In CSV mode suppress the incremental human-readable output so that only
    // the final render goes to stdout (makes piping / redirection clean).
    if (csv_mode) {
        bench.output(nullptr);
    }

    bench_synthetic(bench);
    bench_from_csv(bench, data_dir);

    if (csv_mode) {
        bench.render(ankerl::nanobench::templates::csv(), std::cout);
    }

    return 0;
}
