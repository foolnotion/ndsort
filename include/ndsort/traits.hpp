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

} // namespace ndsort
