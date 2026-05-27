// SPDX-License-Identifier: MIT
#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace ndsort::detail {

template <std::floating_point T>
auto float_to_sortable(T f) -> std::conditional_t<sizeof(T) == 4, uint32_t, uint64_t>
{
    using U = std::conditional_t<sizeof(T) == 4, uint32_t, uint64_t>;
    U bits {};
    std::memcpy(&bits, &f, sizeof(T));
    U const mask = static_cast<U>(-static_cast<U>(bits >> (sizeof(U) * 8 - 1))) | (U { 1 } << (sizeof(U) * 8 - 1));
    return bits ^ mask;
}

template <std::floating_point T>
struct sort_item {
    int index;
    T value;
};

template <std::floating_point T>
struct radix_sort_config {
    using word_t = std::conditional_t<sizeof(T) == 4, uint32_t, uint64_t>;
    static constexpr int radix_bits = sizeof(T) == 4 ? 8 : 11;
    static constexpr int radix_size = 1 << radix_bits;
    static constexpr int radix_mask = radix_size - 1;
    static constexpr int radix_passes = (static_cast<int>(sizeof(word_t)) * 8 + radix_bits - 1) / radix_bits;
    static_assert(radix_passes % 2 == 0, "adjust copy logic if num_passes is odd");
};

template <std::floating_point T>
void radix_sort(sort_item<T>* items, sort_item<T>* scratch, int n)
{
    using config = radix_sort_config<T>;
    sort_item<T>* src = items;
    sort_item<T>* dst = scratch;
    for (int pass = 0; pass < config::radix_passes; ++pass) {
        int const shift = pass * config::radix_bits;
        int counts[config::radix_size] {};
        for (int i = 0; i < n; ++i) {
            ++counts[(float_to_sortable(src[i].value) >> shift) & config::radix_mask];
        }
        int offsets[config::radix_size];
        offsets[0] = 0;
        for (int i = 1; i < config::radix_size; ++i) {
            offsets[i] = offsets[i - 1] + counts[i - 1];
        }
        for (int i = 0; i < n; ++i) {
            int const bucket = static_cast<int>((float_to_sortable(src[i].value) >> shift) & config::radix_mask);
            dst[offsets[bucket]++] = src[i];
        }
        std::swap(src, dst);
    }
}

} // namespace ndsort::detail
