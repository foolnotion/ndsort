// SPDX-License-Identifier: MIT
// Rank-intersect non-dominated sorter.
// Heavy machinery (EVE SIMD, radix sort, packed bitsets) is confined here.

#include "ndsort/rank_intersect.hpp"

#include <eve/module/algo.hpp>

#include <algorithm>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <numeric>
#include <vector>

namespace ndsort {
namespace {

static constexpr uint64_t k_zeros{0UL};
static constexpr uint64_t k_ones{~k_zeros};
static constexpr int      k_digits{std::numeric_limits<uint64_t>::digits};

// Packed-pool budget: ~16 MB covers populations up to ~16k individuals.
static constexpr std::size_t k_pool_budget{2UL << 20};

// IEEE 754 double → sortable uint64_t.
// Positive doubles map to the upper half, negatives to the lower half, both in ascending order.
inline auto float_to_sortable(double f) -> uint64_t
{
    uint64_t bits{};
    std::memcpy(&bits, &f, sizeof(bits));
    uint64_t const mask = -(bits >> 63) | (uint64_t{1} << 63);
    return bits ^ mask;
}

static constexpr int k_radix_bits  = 11;
static constexpr int k_radix_size  = 1 << k_radix_bits;
static constexpr int k_radix_mask  = k_radix_size - 1;
static constexpr int k_key_bits    = 64; // always uint64_t
static constexpr int k_num_passes  = (k_key_bits + k_radix_bits - 1) / k_radix_bits; // 6

struct item {
    int    index;
    double value;
};

void radix_sort(item* items, item* scratch, int n)
{
    item* src = items;
    item* dst = scratch;
    for (int pass = 0; pass < k_num_passes; ++pass) {
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
    // k_num_passes=6 is even → result is already in items, no copy needed
    static_assert(k_num_passes % 2 == 0, "adjust copy logic if num_passes is odd");
}

struct bitset_ref { int lo; int hi; };

// Packed triangular pool: individual j gets (nb - j/k_digits) words.
// Total footprint ≈ n²/128 words.
struct packed_pool {
    std::vector<uint64_t> data;
    std::vector<int>      offsets;

    auto get(int j)       -> uint64_t*       { return data.data() + offsets[j]; }
    auto get(int j) const -> uint64_t const* { return data.data() + offsets[j]; }

    static auto create(int n, int nb) -> packed_pool
    {
        std::vector<int> offs(n);
        int off{0};
        for (int j = 0; j < n; ++j) { offs[j] = off; off += nb - j / k_digits; }
        return packed_pool{std::vector<uint64_t>(static_cast<std::size_t>(off), k_zeros), std::move(offs)};
    }

    static auto total_words(int n, int nb) -> std::size_t
    {
        std::size_t total{0};
        for (int j = 0; j < n; ++j) { total += static_cast<std::size_t>(nb - j / k_digits); }
        return total;
    }
};

// Fallback: heap-allocated per-individual bitsets for very large n.
struct heap_pool {
    std::vector<std::unique_ptr<uint64_t[]>> data;

    auto get(int j)       -> uint64_t*       { return data[j].get(); }
    auto get(int j) const -> uint64_t const* { return data[j].get(); }

    void alloc(int j, int sz)
    {
        data[j] = std::make_unique<uint64_t[]>(static_cast<std::size_t>(sz));
    }
};

template<typename Storage>
auto init_bitsets(std::vector<double> const& fvals, int n, int nb, int obj1_offset,
                  Storage& store, std::vector<bitset_ref>& refs)
    -> std::tuple<std::vector<item>, std::vector<item>, std::vector<uint64_t>>
{
    auto const ub = static_cast<std::size_t>(k_digits * nb) - static_cast<std::size_t>(n);

    std::vector<item> items(n);
    std::vector<item> scratch(n);
    auto const* obj1 = fvals.data() + obj1_offset;
    for (int i = 0; i < n; ++i) { items[i] = {i, obj1[i]}; }
    radix_sort(items.data(), scratch.data(), n);

    std::vector<uint64_t> mask(nb, k_ones);
    mask[nb - 1] >>= ub;

    for (int i = 0; i < n; ++i) {
        auto const j = items[i].index;
        auto const [q, r] = std::div(j, k_digits);
        mask[q] &= ~(uint64_t{1} << static_cast<unsigned>(r));

        int lo{0};
        int hi{nb - q - 1};

        if (n - 1 == i || n - 1 == j) {
            lo = hi + 1;
            refs[j] = {lo, hi};
            continue;
        }

        auto const* ptr = mask.data() + q;
        while (hi >= lo && *(ptr + hi) == k_zeros) { --hi; }

        int const sz = hi - lo + 1;
        if (sz <= 0) {
            lo = hi + 1;
        } else {
            uint64_t* bits{};
            if constexpr (std::is_same_v<Storage, packed_pool>) {
                bits = store.get(j);
            } else {
                store.alloc(j, sz);
                bits = store.get(j);
            }
            bits[0] = (k_ones << static_cast<unsigned>(r)) & mask[q];
            std::copy_n(mask.data() + q + 1, sz - 1, bits + 1);
            while (lo <= hi && bits[lo] == k_zeros) { ++lo; }
            while (lo <= hi && bits[hi] == k_zeros) { --hi; }
        }
        refs[j] = {lo, hi};
    }
    return {std::move(items), std::move(scratch), std::move(mask)};
}

template<typename Storage>
void objective_loop(std::vector<double> const& fvals, int n, int m,
                    Storage& store, std::vector<bitset_ref>& refs,
                    std::vector<item>& items, std::vector<item>& scratch,
                    std::vector<uint64_t>& mask, int nb, std::size_t ub)
{
    for (int obj = 2; obj < m; ++obj) {
        auto const* obj_vals = fvals.data() + static_cast<std::size_t>(obj) * n;
        for (auto& [idx, v] : items) { v = obj_vals[idx]; }
        radix_sort(items.data(), scratch.data(), n);

        std::fill_n(mask.data(), nb, k_ones);
        mask[nb - 1] >>= ub;

        auto done = 0;
        auto const first = items.front().index;
        auto const last  = items.back().index;

        refs[last].lo = refs[last].hi + 1;
        ++done;

        mask[first / k_digits] &= ~(uint64_t{1} << static_cast<unsigned>(first % k_digits));
        done += static_cast<int>(refs[first].lo > refs[first].hi);

        auto mmin = first / k_digits;
        auto mmax = first / k_digits;

        for (auto [i, dummy_] : std::span{items.begin() + 1, items.end() - 1}) {
            auto const [q, r] = std::div(i, k_digits);
            mask[q] &= ~(uint64_t{1} << static_cast<unsigned>(r));
            mmin = std::min(q, mmin);
            mmax = std::max(q, mmax);

            auto& [lo, hi] = refs[i];
            if (lo > hi) { ++done; continue; }

            auto const a = std::max(mmin, lo + q);
            auto const b = std::min(mmax, hi + q);
            if (b < a) { continue; }

            auto* bits = store.get(i);
            std::span<uint64_t>       pb(bits + a - q, b - a + 1);
            std::span<uint64_t const> pm(mask.data() + a, b - a + 1);
            eve::algo::transform_to(eve::views::zip(pb, pm), pb, [](auto t) {
                return kumi::apply(std::bit_and{}, t);
            });
            while (lo <= hi && bits[lo] == k_zeros) { ++lo; }
            while (lo <= hi && bits[hi] == k_zeros) { --hi; }
        }
        if (done == n) { break; }
    }
}

template<typename Storage>
void update_ranks(Storage const& store, std::vector<bitset_ref> const& refs,
                  std::vector<int>& rank,
                  std::vector<std::unique_ptr<uint64_t[]>>& rankset, int n, int nb)
{
    for (int i = 0; i < n; ++i) {
        auto const [lo, hi] = refs[i];
        if (lo > hi) { continue; }
        auto const r = rank[i];
        if (static_cast<std::size_t>(r + 1) == rankset.size()) {
            rankset.push_back(std::make_unique<uint64_t[]>(static_cast<std::size_t>(nb)));
            std::fill_n(rankset.back().get(), nb, uint64_t{0});
        }
        auto*       curr = rankset[r].get();
        auto*       next = rankset[r + 1].get();
        auto const* bits = store.get(i);
        auto const  b    = i / k_digits + lo;

        for (int k = lo, j = b; k <= hi; ++k, ++j) {
            auto x = bits[k] & curr[j];
            if (x == 0UL) { continue; }
            curr[j] &= ~x;
            next[j] |= x;
            auto const o = static_cast<std::size_t>(j) * k_digits;
            for (; x != 0; x &= (x - 1)) {
                ++rank[o + static_cast<std::size_t>(std::countr_zero(x))];
            }
        }
    }
}

} // namespace

auto rank_intersect_sorter::sort_impl(detail::flat_fitness const& ff, double /*eps*/) const -> fronts
{
    // Algorithm requires individuals in lexicographic fitness order.
    auto const perm = detail::lex_perm(ff);
    auto const sff  = detail::reindex(ff, perm);

    int const n  = static_cast<int>(sff.n);
    int const m  = static_cast<int>(sff.m);
    int const nb = n / k_digits + static_cast<int>(n % k_digits != 0);
    auto const ub = static_cast<std::size_t>(k_digits * nb) - static_cast<std::size_t>(n);

    auto const& fvals = sff.data;

    std::vector<bitset_ref> refs(n);
    std::vector<int>        rank(n, 0);

    std::vector<std::unique_ptr<uint64_t[]>> rs;
    rs.push_back(std::make_unique<uint64_t[]>(static_cast<std::size_t>(nb)));
    std::fill_n(rs[0].get(), nb, k_ones);
    rs[0][nb - 1] >>= ub;

    auto const obj1_offset = static_cast<int>(sff.n); // data[1 * n + 0]

    if (packed_pool::total_words(n, nb) <= k_pool_budget) {
        auto pool = packed_pool::create(n, nb);
        auto [items, scratch, mask] = init_bitsets(fvals, n, nb, obj1_offset, pool, refs);
        objective_loop(fvals, n, m, pool, refs, items, scratch, mask, nb, ub);
        update_ranks(pool, refs, rank, rs, n, nb);
    } else {
        heap_pool store;
        store.data.resize(n);
        auto [items, scratch, mask] = init_bitsets(fvals, n, nb, obj1_offset, store, refs);
        objective_loop(fvals, n, m, store, refs, items, scratch, mask, nb, ub);
        update_ranks(store, refs, rank, rs, n, nb);
    }

    auto const rmax = static_cast<std::size_t>(*std::max_element(rank.begin(), rank.end()));
    fronts result(rmax + 1UZ);
    for (std::size_t i = 0; i < static_cast<std::size_t>(n); ++i) {
        result[static_cast<std::size_t>(rank[i])].push_back(perm[i]);
    }
    return result;
}

} // namespace ndsort
