#pragma once

namespace ndsort {

// Traits struct for non-dominated sorters.
// Specialise for a concrete sorter type to advertise its properties.
// Adapters query these at compile time to select optimal strategies.
template<typename S>
struct sorter_traits {
    // Whether this sorter's per-objective passes are independent and can run in parallel.
    static constexpr bool parallel_objectives = false;

    // Whether this sorter requires the population to be lexicographically sorted before calling.
    static constexpr bool requires_sorted_input = false;

    // Whether this sorter produces results identical to DeductiveSorter on all inputs.
    static constexpr bool is_exact = true;
};

// Tag type: pass `presorted` as the last argument to a sorter's operator() to signal
// that the population is already in lexicographic fitness order.  Sorters that
// internally pre-sort will skip that step.
struct presorted_t {};
inline constexpr presorted_t presorted{};

} // namespace ndsort
