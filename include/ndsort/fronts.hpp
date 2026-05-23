#pragma once

#include <cstddef>
#include <vector>

namespace ndsort {

// The canonical return type of all non-dominated sorters:
// a sequence of fronts, each front being a list of indices into the input population.
using fronts = std::vector<std::vector<std::size_t>>;

} // namespace ndsort
