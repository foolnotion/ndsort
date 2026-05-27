#include <nanobind/nanobind.h>
#include <nanobind/ndarray.h>
#include <nanobind/stl/vector.h>

#include <ndsort/ndsort.hpp>

#include <ranges>
#include <span>

namespace nb = nanobind;

// ── Output ────────────────────────────────────────────────────────────────────
// Move each front's vector into a numpy array via a capsule deleter.
// This avoids boxing every index as a Python int: one allocation per front,
// zero per element.
static auto to_py_fronts(ndsort::fronts fronts) -> nb::list
{
    nb::list result;
    for (auto& front : fronts) {
        auto* vec = new std::vector<std::size_t>(std::move(front));
        nb::capsule owner(vec, [](void* p) noexcept {
            delete static_cast<std::vector<std::size_t>*>(p);
        });
        result.append(nb::ndarray<nb::numpy, std::size_t, nb::ndim<1>>(
            vec->data(), {vec->size()}, owner));
    }
    return result;
}

// ── Sort hint ─────────────────────────────────────────────────────────────────
// Mirrors the presorted_t / sorted_unique_t tag types in ndsort::traits.
enum class sort_hint { none, presorted, sorted_unique };

template <typename Sorter, typename Pop>
static auto invoke_sorter(Sorter& sorter, Pop& pop, double eps, sort_hint hint) -> ndsort::fronts
{
    switch (hint) {
    case sort_hint::presorted:
        return sorter(pop, eps, std::identity{}, ndsort::presorted);
    case sort_hint::sorted_unique:
        return sorter(pop, eps, std::identity{}, ndsort::sorted_unique);
    default:
        return sorter(pop, eps);
    }
}

// ── Dispatch ──────────────────────────────────────────────────────────────────
// Fast path (numpy ndarray): a lazy range of std::span<double const> rows lets
// the sorter's flatten() read directly from the numpy buffer.  The only copy
// that occurs is flatten()'s unavoidable row-major → column-major transposition.
//
// Fallback (list-of-lists): copy into std::vector<std::vector<double>> is
// unavoidable since there is no contiguous buffer to borrow from.
template <typename Sorter>
static auto dispatch(nb::object population, double eps, sort_hint hint) -> nb::list
{
    Sorter sorter;

    if (nb::isinstance<nb::ndarray<>>(population)) {
        auto arr = nb::cast<nb::ndarray<double, nb::ndim<2>, nb::c_contig>>(population);
        auto const n = arr.shape(0);
        auto const m = arr.shape(1);
        auto const* ptr = arr.data();
        auto view = std::views::iota(std::size_t{0}, n)
            | std::views::transform([ptr, m](std::size_t i) noexcept {
                  return std::span<double const>(ptr + i * m, m);
              });
        return to_py_fronts(invoke_sorter(sorter, view, eps, hint));
    }

    auto pop = nb::cast<std::vector<std::vector<double>>>(population);
    return to_py_fronts(invoke_sorter(sorter, pop, eps, hint));
}

NB_MODULE(_ext, m)
{
    m.doc() = "Non-dominated sorting algorithms";

    // Register enums before any m.def() that uses them as default argument values.

    nb::enum_<sort_hint>(m, "SortHint")
        .value("none", sort_hint::none,
               "Default: lex-sort and epsilon-deduplicate internally.")
        .value("presorted", sort_hint::presorted,
               "Input is already in lexicographic fitness order; skip internal sort.")
        .value("sorted_unique", sort_hint::sorted_unique,
               "Input is already sorted and contains no duplicates; skip sort and dedup.");

    nb::enum_<ndsort::dominance>(m, "Dominance")
        .value("equal", ndsort::dominance::equal)
        .value("left",  ndsort::dominance::left)
        .value("right", ndsort::dominance::right)
        .value("none",  ndsort::dominance::none);

    // Shared keyword argument with default — must come after the enum is registered.
    auto hint_arg = nb::arg("hint") = sort_hint::none;

    m.def("rank_intersect_sort",    &dispatch<ndsort::rank_intersect_sorter>,
          nb::arg("population"), nb::arg("eps") = 0.0, hint_arg,
          "Rank-Intersection non-dominated sort.");

    m.def("merge_sort",             &dispatch<ndsort::merge_sorter>,
          nb::arg("population"), nb::arg("eps") = 0.0, hint_arg,
          "Merge-based non-dominated sort.");

    m.def("deductive_sort",         &dispatch<ndsort::deductive_sorter>,
          nb::arg("population"), nb::arg("eps") = 0.0, hint_arg,
          "Deductive non-dominated sort.");

    m.def("hierarchical_sort",      &dispatch<ndsort::hierarchical_sorter>,
          nb::arg("population"), nb::arg("eps") = 0.0, hint_arg,
          "Hierarchical non-dominated sort.");

    m.def("best_order_sort",        &dispatch<ndsort::best_order_sorter>,
          nb::arg("population"), nb::arg("eps") = 0.0, hint_arg,
          "Best-Order non-dominated sort.");

    m.def("dominance_degree_sort",  &dispatch<ndsort::dominance_degree_sorter>,
          nb::arg("population"), nb::arg("eps") = 0.0, hint_arg,
          "Dominance Degree non-dominated sort.");

    m.def("rank_ordinal_sort",      &dispatch<ndsort::rank_ordinal_sorter>,
          nb::arg("population"), nb::arg("eps") = 0.0, hint_arg,
          "Rank-Ordinal non-dominated sort.");

    m.def("efficient_binary_sort",  &dispatch<ndsort::efficient_binary_sorter>,
          nb::arg("population"), nb::arg("eps") = 0.0, hint_arg,
          "Efficient Binary non-dominated sort.");

    m.def("efficient_sequential_sort", &dispatch<ndsort::efficient_sequential_sorter>,
          nb::arg("population"), nb::arg("eps") = 0.0, hint_arg,
          "Efficient Sequential non-dominated sort.");

    // pareto_compare: ndarray overload first (zero-copy), list fallback second.
    m.def("pareto_compare",
        [](nb::ndarray<double, nb::ndim<1>, nb::c_contig> a,
           nb::ndarray<double, nb::ndim<1>, nb::c_contig> b, double eps) {
            return ndsort::pareto_compare{}(
                std::span{a.data(), a.shape(0)},
                std::span{b.data(), b.shape(0)},
                eps);
        },
        nb::arg("a"), nb::arg("b"), nb::arg("eps") = 0.0);

    m.def("pareto_compare",
        [](std::vector<double> const& a, std::vector<double> const& b, double eps) {
            return ndsort::pareto_compare{}(a, b, eps);
        },
        nb::arg("a"), nb::arg("b"), nb::arg("eps") = 0.0,
        "Compare two solutions using Pareto dominance.");
}
