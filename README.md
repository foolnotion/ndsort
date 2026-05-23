# ndsort

A generic C++20 library of non-dominated sorting algorithms for multi-objective evolutionary optimization.

## Algorithms

| Sorter | Algorithm | Complexity (best / worst) | Reference |
|:---|:---|:---|:---|
| `deductive_sorter` | Deductive sort | O(MN²) expected / Θ(MN³) | [Mishra & Buzdalov, GECCO 2020](https://doi.org/10.1145/3377930.3390246) |
| `rank_intersect_sorter` | Rank-intersect NDS | O(MN log N) avg / O(MN²) | [Burlacu, arXiv 2022](https://arxiv.org/abs/2203.13654) |
| `merge_sorter` | Merge NDS (MNDS) | O(N log N) best / O(MN²) | [Moreno et al., IEEE TCYB 2020](https://doi.org/10.1109/TCYB.2020.2968301) |
| `hierarchical_sorter` | Hierarchical NDS (HNDS) | O(MN√N) best / O(MN²) | [Bao et al., J. Comput. Sci. 2017](https://doi.org/10.1016/j.jocs.2017.09.015) |
| `best_order_sorter` | Best Order Sort (BOS) | O(MN log N) best / O(MN²) | [Roy et al., GECCO 2016](https://doi.org/10.1145/2908961.2931684) |
| `efficient_binary_sorter` | ENS-BS | O(MN log N) best / O(MN²) | [Zhang et al., IEEE TEC 2015](https://doi.org/10.1109/TEVC.2014.2308305) |
| `efficient_sequential_sorter` | ENS-SS | O(MN√N) best / O(MN²) | [Zhang et al., IEEE TEC 2015](https://doi.org/10.1109/TEVC.2014.2308305) |

Each algorithm is implemented as faithfully as possible to the published version, with
performance improvements (better data structures, early exits, SIMD) that do not alter the output.

### rank_intersect_sorter

The novel contribution of this library.
All other algorithms determine the rank of each solution by *pulling*: either scanning
fronts to find the earliest one the solution can join (ENS), or finding the maximum rank
among the solution's dominators (MNDS).
`rank_intersect_sorter` instead *pushes*: for each solution i at rank r, it intersects
i's dominance bitset with a live "rank-r membership" bitset (`rankset[r]`), and promotes
all dominated same-rank solutions to r+1 atomically — 64 at a time via bitwise AND.

The per-objective dominance sets (triangular packed bitsets built via radix sort) and
the SIMD intersection (via [EVE](https://github.com/jfalcou/eve)) are standard techniques
applied in service of this push-based rank propagation.
The rankset intersection mechanism is the novel claim; see the
[arXiv preprint](https://arxiv.org/abs/2203.13654) for details.

## Usage

```cpp
#include <ndsort/ndsort.hpp>

// Any random-access range of random-access ranges of scalars works.
std::vector<std::vector<double>> population = /* ... */;

// Each sorter is a callable object.
ndsort::fronts f = ndsort::rank_intersect_sorter{}(population);

// f[0] is the first (non-dominated) front — indices into population.
// f[1] is the second front, etc.
for (auto idx : f[0]) {
    // population[idx] is a Pareto-optimal solution
}
```

All sorters share the same signature:

```cpp
auto operator()(population, double eps = 0.0, Proj proj = {}) -> ndsort::fronts;
```

`eps` enables ε-dominance (`a[k] <= b[k] + eps` for all k).
`proj` is a projection applied to each element before extracting its fitness vector,
useful when the population holds structs rather than raw vectors:

```cpp
struct Individual { std::vector<double> fitness; /* ... */ };
std::vector<Individual> pop = /* ... */;

auto f = ndsort::rank_intersect_sorter{}(pop, 0.0, &Individual::fitness);
```

### ENS sorters

`efficient_binary_sorter` and `efficient_sequential_sorter` require the population to be
lexicographically sorted. The default `operator()` handles this automatically; if the
population is already sorted, pass the `presorted` tag to skip the re-sort:

```cpp
auto f = ndsort::efficient_binary_sorter{}(pop, 0.0, std::identity{}, ndsort::presorted);
```

### eps_adapter

Burns in a fixed ε so call sites don't need to pass it explicitly:

```cpp
auto sorter = ndsort::eps_adapter{ndsort::rank_intersect_sorter{}, 1e-6};
auto f = sorter(population);
```

## Input contract

All sorters expect the input to be **free of exact duplicates**
(solutions with identical objective vectors).
Duplicate handling is left to the caller: in a typical MOEA this means deduplicating
after each generation before ranking.
Passing duplicates may place them in consecutive fronts rather than the same front —
behaviour that varies by sorter and is not treated as a bug.

## Building

Requires a C++20 compiler and [CMake](https://cmake.org/) ≥ 3.14.
Dependencies ([EVE](https://github.com/jfalcou/eve), [Catch2](https://github.com/catchorg/Catch2))
are managed via [vcpkg](https://github.com/microsoft/vcpkg).

```sh
cmake -B build -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build
```

### CMake options

| Option | Default | Description |
|:---|:---|:---|
| `BUILD_TESTING` | `ON` (dev mode) | Build and run the test suite |
| `BUILD_BENCHMARKS` | `ON` (dev mode) | Build the nanobench benchmark binary |
| `BUILD_EXAMPLES` | `ON` (dev mode) | Build example programs |

### Benchmarks

```sh
cmake --build build --target ndsort_bench
python3 scripts/generate_benchmarks.py \
    --binary build/benchmark/ndsort_bench \
    --data   test/data
```

This regenerates `BENCHMARKS.md` from a fresh run.
Use `--save-csv results.csv` to cache raw output and re-render later with `--from-csv results.csv`.
See [BENCHMARKS.md](BENCHMARKS.md) for results on an AMD Ryzen 9 5950X.

## License

MIT
