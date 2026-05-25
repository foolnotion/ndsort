// SPDX-License-Identifier: MIT
#pragma once

namespace ndsort {

// Traits struct for non-dominated sorters.
// Specialise for a concrete sorter type to advertise its properties.
// Adapters query these at compile time to select optimal strategies.
template <typename S>
struct sorter_traits {
    // Whether this sorter's per-objective passes are independent and can run in parallel.
    static constexpr bool parallel_objectives = false;

    // Whether this sorter's underlying algorithm requires lex-sorted input.
    // The default operator() satisfies this internally; pass presorted_t to skip that step
    // when the caller has already sorted the population.
    static constexpr bool requires_sorted_input = false;

    // Whether this sorter produces results identical to DeductiveSorter on all inputs.
    static constexpr bool is_exact = true;
};

// Tag type: pass `presorted` as the last argument to a sorter's operator() to signal
// that the population is already in lexicographic fitness order.  Sorters that
// internally pre-sort will skip that step.
struct presorted_t {};
inline constexpr presorted_t presorted {};

// Tag type: pass `sorted_unique` as the last argument to a sorter's operator() to signal
// that the population is already lexicographically sorted AND contains no duplicates.
// Sorters will skip both the internal lex-sort and the epsilon-deduplication passes.
// Use this when the caller has already performed sorting and duplicate removal.
struct sorted_unique_t {};
inline constexpr sorted_unique_t sorted_unique {};

} // namespace ndsort
