// SPDX-License-Identifier: MIT

#include "ndsort/merge_sort.hpp"
#include "ndsort/detail/sorting_primitives.hpp"

#include <eve/module/algo.hpp>

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace ndsort {
namespace {

    class bitset_manager {
        using word_t = uint64_t;
        static constexpr std::size_t first_idx = 0;
        static constexpr std::size_t last_idx = 1;
        static constexpr word_t ones = ~word_t { 0 };
        static constexpr std::size_t word_bits = std::numeric_limits<word_t>::digits;

        std::vector<std::vector<word_t>> bitsets_;
        std::vector<std::array<std::size_t, 2>> ranges_;
        std::vector<int> word_ranking_;
        std::vector<int> ranking_;
        std::vector<int> ranking0_;
        int max_rank_ { 0 };
        std::vector<word_t> incremental_;
        std::size_t inc_first_ { std::numeric_limits<std::size_t>::max() };
        std::size_t inc_last_ { 0 };

    public:
        explicit bitset_manager(std::size_t n)
            : bitsets_(n)
            , ranges_(n)
            , word_ranking_(n, 0)
            , ranking_(n, 0)
            , ranking0_(n, 0)
            , incremental_(n / word_bits + (n % word_bits != 0 ? 1UZ : 0UZ), word_t { 0 })
        {
        }

        [[nodiscard]] auto get_ranking() const -> std::vector<int> const& { return ranking0_; }

        auto update_solution_dominance(std::size_t id) -> bool
        {
            auto fw = ranges_[id][first_idx];
            auto lw = ranges_[id][last_idx];
            lw = std::min(lw, inc_last_);
            fw = std::max(fw, inc_first_);

            while (fw <= lw && (bitsets_[id][fw] & incremental_[fw]) == 0) {
                ++fw;
            }
            while (fw <= lw && (bitsets_[id][lw] & incremental_[lw]) == 0) {
                --lw;
            }
            ranges_[id][first_idx] = fw;
            ranges_[id][last_idx] = lw;

            if (fw > lw) {
                return false;
            }

            auto* bits = bitsets_[id].data();
            std::span<word_t> pb(bits + fw, lw - fw + 1);
            std::span<word_t const> pm(incremental_.data() + fw, lw - fw + 1);
            eve::algo::transform_to(eve::views::zip(pb, pm), pb, [](auto t) {
                return kumi::apply(std::bit_and {}, t);
            });
            return true;
        }

        void compute_solution_ranking(std::size_t id, std::size_t init_id)
        {
            auto fw = ranges_[id][first_idx];
            auto lw = ranges_[id][last_idx];
            lw = std::min(lw, inc_last_);
            fw = std::max(fw, inc_first_);
            if (fw > lw) {
                return;
            }

            int rank { 0 };
            for (auto w = fw; w <= lw; ++w) {
                word_t word = bitsets_[id][w] & incremental_[w];
                if (word == 0) {
                    continue;
                }
                auto i = static_cast<std::size_t>(std::countr_zero(word));
                auto const offset = w * word_bits;
                do {
                    auto r = ranking_[offset + i];
                    if (r >= rank) {
                        rank = r + 1;
                    }
                    ++i;
                    if (i < word_bits) {
                        i += static_cast<std::size_t>(std::countr_zero(word >> i));
                    }
                } while (i < word_bits && rank <= word_ranking_[w]);
                if (rank > max_rank_) {
                    max_rank_ = rank;
                    break;
                }
            }
            ranking_[id] = rank;
            ranking0_[init_id] = rank;
            auto wi = id / word_bits;
            if (rank > word_ranking_[wi]) {
                word_ranking_[wi] = rank;
            }
        }

        void update_incremental(std::size_t id)
        {
            auto wi = id / word_bits;
            incremental_[wi] |= (word_t { 1 } << (id % word_bits));
            inc_last_ = std::max(inc_last_, wi);
            inc_first_ = std::min(inc_first_, wi);
        }

        auto init_solution_bitset(std::size_t id) -> bool
        {
            auto wi = id / word_bits;
            if (wi < inc_first_ || id == 0) {
                ranges_[id][first_idx] = std::numeric_limits<std::size_t>::max();
                return false;
            }
            if (wi == inc_first_) {
                bitsets_[id].resize(wi + 1);
                auto intersection = incremental_[inc_first_] & ~(ones << (id % word_bits));
                if (intersection != 0) {
                    ranges_[id][first_idx] = wi;
                    ranges_[id][last_idx] = wi;
                    bitsets_[id][wi] = intersection;
                }
                return intersection != 0;
            }
            auto lw = std::min(inc_last_, wi);
            ranges_[id][first_idx] = inc_first_;
            ranges_[id][last_idx] = lw;
            bitsets_[id] = std::vector<word_t>(lw + 1);
            std::copy_n(incremental_.data() + inc_first_, lw - inc_first_ + 1,
                bitsets_[id].data() + inc_first_);
            if (inc_last_ >= wi) {
                bitsets_[id][lw] = incremental_[lw] & ~(ones << (id % word_bits));
                if (bitsets_[id][lw] == 0) {
                    --ranges_[id][last_idx];
                }
            }
            return true;
        }

        void clear_incremental()
        {
            std::fill(incremental_.begin(), incremental_.end(), word_t { 0 });
            inc_last_ = 0;
            inc_first_ = std::numeric_limits<std::size_t>::max();
        }
    };

    template <typename T>
    auto merge_sort_impl(detail::flat_fitness<T> const& ff) -> fronts
    {
        std::size_t const n = ff.n;
        std::size_t const m = ff.m;
        int const ni = static_cast<int>(n);

        if (m == 1) {
            fronts result(n);
            for (std::size_t i = 0; i < n; ++i) {
                result[i].push_back(i);
            }
            return result;
        }

        bitset_manager bsm(n);

        std::vector<detail::sort_item<T>> items(n), scratch(n);
        for (std::size_t i = 0; i < n; ++i) {
            items[i] = { static_cast<int>(i), ff.at(1, i) };
        }
        detail::radix_sort(items.data(), scratch.data(), ni);

        bool any_dominance { true };
        for (std::size_t obj = 1; obj < m; ++obj) {
            if (obj > 1) {
                for (auto& [idx, v] : items) {
                    v = ff.at(obj, static_cast<std::size_t>(idx));
                }

                bool already_sorted { true };
                for (int i = 0; i + 1 < ni && already_sorted; ++i) {
                    already_sorted = (items[i].value <= items[i + 1].value);
                }
                if (already_sorted) {
                    if (obj == m - 1) {
                        for (std::size_t i = 0; i < n; ++i) {
                            auto j = static_cast<std::size_t>(items[i].index);
                            bsm.compute_solution_ranking(j, j);
                        }
                    }
                    continue;
                }

                detail::radix_sort(items.data(), scratch.data(), ni);
                bsm.clear_incremental();
            }

            any_dominance = false;
            for (std::size_t i = 0; i < n; ++i) {
                auto const j = static_cast<std::size_t>(items[i].index);
                if (obj == 1) {
                    any_dominance |= bsm.init_solution_bitset(j);
                } else if (obj < m - 1) {
                    any_dominance |= bsm.update_solution_dominance(j);
                }
                if (obj == m - 1) {
                    bsm.compute_solution_ranking(j, j);
                }
                bsm.update_incremental(j);
            }
            if (!any_dominance) {
                break;
            }
        }

        auto const& rank_u = bsm.get_ranking();
        auto const rmax = static_cast<std::size_t>(*std::max_element(rank_u.begin(), rank_u.end()));
        fronts result(rmax + 1UZ);
        for (std::size_t i = 0; i < n; ++i) {
            result[static_cast<std::size_t>(rank_u[i])].push_back(i);
        }
        return result;
    }

} // namespace

template <typename T>
auto merge_sorter::sort_impl(detail::flat_fitness<T> const& ff, double) const -> fronts
{
    return merge_sort_impl(ff);
}

template NDSORT_EXPORT auto merge_sorter::sort_impl<float>(detail::flat_fitness<float> const&, double) const -> fronts;
template NDSORT_EXPORT auto merge_sorter::sort_impl<double>(detail::flat_fitness<double> const&, double) const -> fronts;

} // namespace ndsort
