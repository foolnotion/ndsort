# ndsort — Design Document

A standalone C++20 library for non-dominated sorting (Pareto-front decomposition),
extracted from Operon and generalised to work with any fitness representation.

---

## Motivation

Non-dominated sorting is a core primitive in multi-objective evolutionary algorithms.
The implementations in Operon are already decoupled from tree-based symbolic regression
in all but their type signatures — they only read fitness values and return front indices.
Extracting them into a dedicated library makes them reusable by other C++ EA frameworks
and enables independent versioning, testing, and benchmarking.

---

## Design goals

1. **Zero coupling** — no dependency on Operon types; works with any range of fitness vectors.
2. **Zero overhead** — sorters are value types; no virtual dispatch on the hot path.
3. **Composable** — behavior (epsilon-dominance, parallelism, projection) is added through
   adapters, not inheritance.
4. **Concept-constrained** — all interfaces are expressed in C++20 concepts; errors are
   caught at the call site, not inside a template instantiation chain.
5. **Projection-aware** — sorters accept an optional projection, following the design of
   cpp-sort and `std::ranges` algorithms, so they work directly on structs that carry
   fitness as a member.

---

## Inspiration: cpp-sort

[cpp-sort](https://github.com/Morwenn/cpp-sort) offers three ideas worth adopting directly:

### 1. Sorters as types, not functions

Each algorithm is an empty struct with a templated `operator()`. This makes sorters
first-class values: they can be stored in variables, passed as template arguments, and
composed at zero cost.

```cpp
// cpp-sort pattern
struct merge_sorter {
    template<std::ranges::random_access_range R, typename Cmp = std::less<>>
    auto operator()(R&& range, Cmp cmp = {}) const -> void;
};
```

For ndsort the return type is front indices rather than a sorted range, but the shape is
the same.

### 2. Projections

Every sorter overload accepts an optional projection that transforms each element before
the fitness comparison. This removes the need for wrapper types or adapter shims:

```cpp
// sort a vector of your own structs directly
std::vector<Individual> pop = ...;
auto fronts = ndsort::RankIntersect{}(pop, 0.0, &Individual::fitness);
```

The projection must map an element to something that satisfies `FitnessVector`.

### 3. Adapter/decorator pattern

Adapters are class templates that wrap a sorter and modify its behavior. They are
composed at the type level — no runtime cost, no inheritance hierarchy.

```cpp
// cpp-sort adapter shape
template<Sorter S>
struct some_adapter {
    S sorter_;
    template<...> auto operator()(...) const;
};
```

ndsort provides adapters for epsilon-dominance and parallel objective passes (see below).

---

## Concepts

```cpp
namespace ndsort {

// A scalar type that can be compared with <
template<typename T>
concept Scalar = std::totally_ordered<T>;

// A fitness vector: contiguous, random-access, fixed size at runtime
template<typename F>
concept FitnessVector =
    std::ranges::random_access_range<F> &&
    Scalar<std::ranges::range_value_t<F>>;

// Projection maps an element to its fitness vector
template<typename Proj, typename Element>
concept FitnessProjection =
    std::invocable<Proj, Element> &&
    FitnessVector<std::invoke_result_t<Proj, Element>>;

// A population is a sized random-access range of elements that have fitness
template<typename P, typename Proj = std::identity>
concept Population =
    std::ranges::random_access_range<P> &&
    std::ranges::sized_range<P> &&
    FitnessProjection<Proj, std::ranges::range_value_t<P>>;

// The canonical result type: a vector of fronts, each front a vector of indices
using Fronts = std::vector<std::vector<std::size_t>>;

// A non-dominated sorter: callable with (population, eps, projection)
// The third overload set (with just population) uses identity projection and eps=0
template<typename S, typename P, typename Proj = std::identity>
concept NondominatedSorter =
    Population<P, Proj> &&
    requires(S s, P pop, double eps, Proj proj) {
        { s(pop, eps, proj) } -> std::same_as<Fronts>;
        { s(pop, eps)       } -> std::same_as<Fronts>;  // identity projection
        { s(pop)            } -> std::same_as<Fronts>;  // eps=0, identity projection
    };

} // namespace ndsort
```

---

## Implementation split (pimpl at the boundary)

The templated public interface and the SIMD/algorithm implementation are separated
at a canonical internal type. This keeps all heavy headers (EVE, bitset machinery)
out of the consumer's translation unit.

### Canonical fitness representation

The internal type is a flat, row-major SoA buffer of sortable unsigned integers,
paired with dimensions:

```cpp
// detail/flat_fitness.hpp  (no EVE dependency)
namespace ndsort::detail {

struct FlatFitness {
    std::vector<std::uint32_t> data; // fvals[obj * n + i], float→sortable-uint
    std::size_t n{};                 // number of individuals
    std::size_t m{};                 // number of objectives
};

// Reads any Population through any projection and fills a FlatFitness.
// This is the only templated code that touches the caller's types.
template<Population<Proj> P, typename Proj = std::identity>
auto flatten(P&& pop, double eps, Proj proj = {}) -> FlatFitness;

} // namespace ndsort::detail
```

`flatten` is defined in a detail header included only by the sorter headers.
It has no dependency beyond the C++20 standard library.

### Split sorter pattern

Each sorter has:
- A **public header** declaring a struct that inherits from `detail::sorter_base<Derived>`.
  The CRTP base provides all three `operator()` overloads (default, `presorted_t`, `sorted_unique_t`),
  delegating to the derived class's private `sort_impl`.
- A **compiled `.cpp`** that defines `sort_impl` and freely includes EVE,
  bitset machinery, or any other heavy implementation header.

```cpp
// include/ndsort/rank_intersect.hpp
#include <ndsort/detail/sorter_base.hpp>

namespace ndsort {

struct NDSORT_EXPORT rank_intersect_sorter : detail::sorter_base<rank_intersect_sorter> {
private:
    // Defined in rank_intersect.cpp — EVE never leaks into this header.
    template <typename T>
    auto sort_impl(detail::flat_fitness<T> const&, double eps) const -> fronts;

    friend struct detail::sorter_base<rank_intersect_sorter>;
};

} // namespace ndsort
```

```cpp
// source/rank_intersect.cpp
#include <eve/module/algo.hpp>   // SIMD — only here, never in the header
#include <ndsort/rank_intersect.hpp>
#include <ndsort/detail/flat_fitness.hpp>

namespace ndsort {

auto RankIntersect::sort_impl(detail::FlatFitness const& ff, double eps) const -> Fronts {
    // PackedPool, radix sort, SIMD bitset intersection ...
}

} // namespace ndsort
```

The consumer sees only `<ndsort/rank_intersect.hpp>`, which pulls in `flat_fitness.hpp`
(stdlib-only) and the concept headers. EVE is an implementation detail of the compiled
library, invisible at the include boundary.

This also means `FlatFitness` can use `uint32_t` or `uint64_t` depending on the scalar
width of the input (detected in `flatten`), without exposing that choice in the API.

### Float-to-sortable encoding

`flatten` encodes each fitness scalar as a sortable unsigned integer so the radix sort
in RankIntersect sees only integer keys:

```cpp
// Works for both float (32-bit) and double (64-bit) via std::conditional_t
template<std::floating_point T>
auto float_to_sortable(T v) -> std::conditional_t<sizeof(T)==4, uint32_t, uint64_t> {
    using U = std::conditional_t<sizeof(T)==4, uint32_t, uint64_t>;
    U bits{};
    std::memcpy(&bits, &v, sizeof(T));
    // flip all bits if negative, else flip only the sign bit (IEEE 754 trick)
    U mask = (bits >> (sizeof(U)*8 - 1)) ? ~U{0} : (U{1} << (sizeof(U)*8 - 1));
    return bits ^ mask;
}
```

Sorters that don't use radix sort (Deductive, MergeSort) can skip this encoding and
work directly from the canonical `double` representation; `flatten` can be parameterised
on the key type.

---

## Sorter interface

Each sorter is a stateless struct with a templated call operator.
The full signature:

```cpp
template<Population<Proj> P, typename Proj = std::identity>
auto operator()(P&& pop, double eps = 0.0, Proj proj = {}) const -> Fronts;
```

`flatten` is called exactly once inside `operator()`, so the input range is traversed
once regardless of projection cost. Everything after that is integer arithmetic on the
canonical buffer.

### Tag dispatch

All sorters provide three `operator()` overloads selected by an optional trailing tag:

```cpp
// Full pipeline: lex-sort, eps-dedup, sort, expand indices.
auto f = sorter(pop, eps, proj);                     // or just sorter(pop)

// Presorted: skip lex-sort, still run eps-dedup.
auto f = sorter(pop, eps, proj, ndsort::presorted);

// Sorted-unique: skip both lex-sort and eps-dedup — fastest path.
auto f = sorter(pop, eps, proj, ndsort::sorted_unique);
```

The boilerplate is provided once by a CRTP base (`detail::sorter_base<Derived>`);
each concrete sorter only implements a private `sort_impl(flat_fitness<T> const&, double eps)` method.

### Provided sorters

| Type                    | Algorithm                       | Best case  |
|-------------------------|---------------------------------|------------|
| `RankIntersect`         | Bitset intersection + radix sort | low m, large n |
| `MergeSort`             | Bitset merge                    | low–mid m  |
| `Deductive`             | Bitset sweep                    | reference  |
| `DominanceDegree`       | Degree matrix                   | mid m      |
| `EfficientBinary`       | ENS-BS                          | m=2        |
| `EfficientSequential`   | ENS-SS                          | m=2        |
| `BestOrder`             | BOS                             | m=2        |
| `HierarchicalSort`      | HSS                             | mid m      |
| `RankOrdinal`           | Ordinal rank                    | small n    |

---

## Adapters

Adapters are class templates that take a sorter type and return a new sorter type.
They follow the same callable interface as raw sorters so they satisfy
`NondominatedSorter` and can themselves be adapted.

### `eps_adapter<S>` — epsilon-dominance

Burns in a fixed epsilon so call sites don't need to pass it explicitly.
Useful when constructing sorters in algorithm configuration structs.

```cpp
template<NondominatedSorter S>
struct eps_adapter {
    S sorter_{};
    double eps_{};

    template<Population<Proj> P, typename Proj = std::identity>
    auto operator()(P&& pop, Proj proj = {}) const -> Fronts {
        return sorter_(std::forward<P>(pop), eps_, proj);
    }
};

// usage
auto sorter = ndsort::eps_adapter<ndsort::RankIntersect>{ .eps_ = 1e-6 };
auto fronts = sorter(population);
```

### `cached_adapter<S>` — result caching

Caches the last result keyed by population size and content hash.
Useful when the same population is sorted multiple times per generation
(e.g., during reinsertion).

```cpp
template<NondominatedSorter S>
struct cached_adapter {
    S sorter_{};
    mutable std::optional<Fronts> cache_;
    mutable std::size_t last_hash_{};
    // ...
};
```

### `parallel_adapter<S>` *(future)*

Runs the per-objective passes of a sorter in parallel using `std::execution::par_unseq`
or taskflow. Not all sorters expose parallelisable structure; the adapter detects this
via a trait `sorter_traits<S>::parallel_objectives`.

---

## Sorter traits

Inspired by `cpp-sort::sorter_traits`. Each sorter specialises a traits struct to
advertise its properties. Adapters query these at compile time.

```cpp
namespace ndsort {

template<typename S>
struct sorter_traits {
    static constexpr bool parallel_objectives = false;
    static constexpr bool requires_sorted_input = false;
    static constexpr bool is_exact = true;
};

// Tag types for preprocessing control:
struct presorted_t {};       // input is lex-sorted, may have duplicates
inline constexpr presorted_t presorted {};
struct sorted_unique_t {};   // input is lex-sorted AND deduplicated
inline constexpr sorted_unique_t sorted_unique {};

} // namespace ndsort
```

---

## Operon integration

After extraction, Operon's `NondominatedSorterBase` hierarchy is replaced by a thin
shim that adapts the new generic interface. The projection extracts `Individual::Fitness`:

```cpp
// in operon — adapts ndsort to the existing Operon call sites
inline constexpr auto fitness_proj = [](Operon::Individual const& ind)
    -> Operon::Span<Operon::Scalar const> {
        return { ind.Fitness.data(), ind.Fitness.size() };
    };

// NSGA2 already stable_partitions duplicates before ranking, so use sorted_unique.
fronts_ = (*sorter_)(uniq, eps, fitness_proj, ndsort::sorted_unique);
```

The virtual `NondominatedSorterBase` can be kept temporarily as a compatibility shim
that type-erases a concrete ndsort sorter, or removed once all call sites are updated.

---

## Repository layout

```
ndsort/
├── CMakeLists.txt
├── include/
│   └── ndsort/
│       ├── ndsort.hpp              # umbrella include
│       ├── concepts.hpp            # FitnessVector, Population, NondominatedSorter
│       ├── traits.hpp              # sorter_traits
│       ├── fronts.hpp              # Fronts type alias + helpers
│       ├── rank_intersect.hpp      # public interface only — no EVE
│       ├── merge_sort.hpp
│       ├── deductive.hpp
│       ├── dominance_degree.hpp
│       ├── efficient_sort.hpp
│       ├── best_order_sort.hpp
│       ├── hierarchical_sort.hpp
│       ├── rank_ordinal.hpp
│       ├── adapters/
│       │   ├── eps_adapter.hpp
│       │   └── cached_adapter.hpp
│       └── detail/
│           ├── flat_fitness.hpp    # FlatFitness + flatten<>; stdlib-only
│           └── sorter_base.hpp    # CRTP base providing operator() overloads
├── source/
│   ├── rank_intersect.cpp          # EVE SIMD, radix sort, PackedPool — all here
│   ├── merge_sort.cpp
│   ├── deductive.cpp
│   └── ...
├── test/
│   ├── CMakeLists.txt
│   ├── correctness.cpp             # agreement with DeductiveSorter, seed sweep
│   └── performance.cpp             # nanobench microbenchmarks
└── flake.nix                       # Nix dev shell with EVE, Catch2, nanobench
```

The `detail/` subdirectory is part of the public install tree (consumers need `flatten`
to compile the templated `operator()`), but its contents are not part of the stable API.

---

## Dependencies

| Dependency    | Role                                   | Required |
|---------------|----------------------------------------|----------|
| EVE           | SIMD bitset ops in RankIntersect       | for RS only |
| Catch2        | unit + correctness tests               | test only |
| nanobench     | performance benchmarks                 | test only |
| cpp-sort      | removed (was used by MergeSort internally, replaced) | — |

The core library headers (`concepts.hpp`, `traits.hpp`, `fronts.hpp`) have zero
third-party dependencies — only the C++20 standard library.

---

## Open questions

1. **`FlatFitness` key width** — `flatten` currently encodes to `uint32_t` (for `float`
   input). Sorters that don't radix-sort don't need integer keys at all; `flatten` could
   be parameterised on a key type or provide two overloads (`flatten_sortable` vs
   `flatten_doubles`). Decide when porting Deductive/MergeSort.

2. **Return type flexibility** — `Fronts = vector<vector<size_t>>` is concrete and
   simple. If small-vector optimisation matters later, it can be a template parameter on
   the sorter. Start concrete.

3. **Epsilon as a type parameter vs runtime value** — `eps_adapter` burns it in at
   construction time; raw sorters take it at call time. This split seems right and mirrors
   how cpp-sort handles comparators vs adapters.

4. **cpp-sort removal from MergeSort** — `merge_sort.cpp` still depends on
   `<cpp-sort/sorters/merge_sorter.h>`. The internal merge step should be replaced
   with `std::ranges::inplace_merge` or a hand-rolled merge to make ndsort dependency-free.

5. **CMake install interface** — `EVE` must appear only in the private link dependencies
   of the compiled targets, not in the public interface. `flat_fitness.hpp` (stdlib-only)
   goes in the public include path. This needs a careful `target_link_libraries` split
   between `PUBLIC` (concepts, fronts, detail/flat_fitness) and `PRIVATE` (EVE).
