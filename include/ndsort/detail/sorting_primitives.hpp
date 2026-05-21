#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>

namespace ndsort::detail {

// IEEE 754 double → sortable uint64_t (positive values map above negatives, both ascending).
inline auto float_to_sortable(double f) -> uint64_t
{
    uint64_t bits{};
    std::memcpy(&bits, &f, sizeof(bits));
    uint64_t const mask = -(bits >> 63) | (uint64_t{1} << 63);
    return bits ^ mask;
}

struct sort_item {
    int    index;
    double value;
};

static constexpr int k_radix_bits = 11;
static constexpr int k_radix_size = 1 << k_radix_bits;
static constexpr int k_radix_mask = k_radix_size - 1;
static constexpr int k_radix_passes = (64 + k_radix_bits - 1) / k_radix_bits; // 6

static_assert(k_radix_passes % 2 == 0, "adjust copy logic if num_passes is odd");

inline void radix_sort(sort_item* items, sort_item* scratch, int n)
{
    sort_item* src = items;
    sort_item* dst = scratch;
    for (int pass = 0; pass < k_radix_passes; ++pass) {
        int const shift = pass * k_radix_bits;
        int counts[k_radix_size]{};
        for (int i = 0; i < n; ++i) {
            ++counts[(float_to_sortable(src[i].value) >> shift) & k_radix_mask];
        }
        int offsets[k_radix_size];
        offsets[0] = 0;
        for (int i = 1; i < k_radix_size; ++i) { offsets[i] = offsets[i - 1] + counts[i - 1]; }
        for (int i = 0; i < n; ++i) {
            int const bucket = static_cast<int>((float_to_sortable(src[i].value) >> shift) & k_radix_mask);
            dst[offsets[bucket]++] = src[i];
        }
        std::swap(src, dst);
    }
    // k_radix_passes=6 is even → result is already in items, no copy needed
}

} // namespace ndsort::detail
