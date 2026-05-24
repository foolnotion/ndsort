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
        static constexpr std::size_t k_first = 0;
        static constexpr std::size_t k_last = 1;
        static constexpr word_t k_ones = ~word_t { 0 };
        static constexpr std::size_t k_word_bits = std::numeric_limits<word_t>::digits;

        std::vector<std::vector<word_t>> m_bitsets;
        std::vector<std::array<std::size_t, 2>> m_ranges;
        std::vector<int> m_word_ranking;
        std::vector<int> m_ranking;
        std::vector<int> m_ranking0;
        int m_max_rank { 0 };
        std::vector<word_t> m_incremental;
        std::size_t m_inc_first { std::numeric_limits<std::size_t>::max() };
        std::size_t m_inc_last { 0 };

    public:
        explicit bitset_manager(std::size_t n)
            : m_bitsets(n)
            , m_ranges(n)
            , m_word_ranking(n, 0)
            , m_ranking(n, 0)
            , m_ranking0(n, 0)
            , m_incremental(n / k_word_bits + (n % k_word_bits != 0 ? 1UZ : 0UZ), word_t { 0 })
        {
        }

        [[nodiscard]] auto get_ranking() const -> std::vector<int> const& { return m_ranking0; }

        auto update_solution_dominance(std::size_t id) -> bool
        {
            auto fw = m_ranges[id][k_first];
            auto lw = m_ranges[id][k_last];
            lw = std::min(lw, m_inc_last);
            fw = std::max(fw, m_inc_first);

            while (fw <= lw && (m_bitsets[id][fw] & m_incremental[fw]) == 0) {
                ++fw;
            }
            while (fw <= lw && (m_bitsets[id][lw] & m_incremental[lw]) == 0) {
                --lw;
            }
            m_ranges[id][k_first] = fw;
            m_ranges[id][k_last] = lw;

            if (fw > lw) {
                return false;
            }

            auto* bits = m_bitsets[id].data();
            std::span<word_t> pb(bits + fw, lw - fw + 1);
            std::span<word_t const> pm(m_incremental.data() + fw, lw - fw + 1);
            eve::algo::transform_to(eve::views::zip(pb, pm), pb, [](auto t) {
                return kumi::apply(std::bit_and {}, t);
            });
            return true;
        }

        void compute_solution_ranking(std::size_t id, std::size_t init_id)
        {
            auto fw = m_ranges[id][k_first];
            auto lw = m_ranges[id][k_last];
            lw = std::min(lw, m_inc_last);
            fw = std::max(fw, m_inc_first);
            if (fw > lw) {
                return;
            }

            int rank { 0 };
            for (auto w = fw; w <= lw; ++w) {
                word_t word = m_bitsets[id][w] & m_incremental[w];
                if (word == 0) {
                    continue;
                }
                auto i = static_cast<std::size_t>(std::countr_zero(word));
                auto const offset = w * k_word_bits;
                do {
                    auto r = m_ranking[offset + i];
                    if (r >= rank) {
                        rank = r + 1;
                    }
                    ++i;
                    if (i < k_word_bits) {
                        i += static_cast<std::size_t>(std::countr_zero(word >> i));
                    }
                } while (i < k_word_bits && rank <= m_word_ranking[w]);
                if (rank > m_max_rank) {
                    m_max_rank = rank;
                    break;
                }
            }
            m_ranking[id] = rank;
            m_ranking0[init_id] = rank;
            auto wi = id / k_word_bits;
            if (rank > m_word_ranking[wi]) {
                m_word_ranking[wi] = rank;
            }
        }

        void update_incremental(std::size_t id)
        {
            auto wi = id / k_word_bits;
            m_incremental[wi] |= (word_t { 1 } << (id % k_word_bits));
            m_inc_last = std::max(m_inc_last, wi);
            m_inc_first = std::min(m_inc_first, wi);
        }

        auto init_solution_bitset(std::size_t id) -> bool
        {
            auto wi = id / k_word_bits;
            if (wi < m_inc_first || id == 0) {
                m_ranges[id][k_first] = std::numeric_limits<std::size_t>::max();
                return false;
            }
            if (wi == m_inc_first) {
                m_bitsets[id].resize(wi + 1);
                auto intersection = m_incremental[m_inc_first] & ~(k_ones << (id % k_word_bits));
                if (intersection != 0) {
                    m_ranges[id][k_first] = wi;
                    m_ranges[id][k_last] = wi;
                    m_bitsets[id][wi] = intersection;
                }
                return intersection != 0;
            }
            auto lw = std::min(m_inc_last, wi);
            m_ranges[id][k_first] = m_inc_first;
            m_ranges[id][k_last] = lw;
            m_bitsets[id] = std::vector<word_t>(lw + 1);
            std::copy_n(m_incremental.data() + m_inc_first, lw - m_inc_first + 1,
                m_bitsets[id].data() + m_inc_first);
            if (m_inc_last >= wi) {
                m_bitsets[id][lw] = m_incremental[lw] & ~(k_ones << (id % k_word_bits));
                if (m_bitsets[id][lw] == 0) {
                    --m_ranges[id][k_last];
                }
            }
            return true;
        }

        void clear_incremental()
        {
            std::fill(m_incremental.begin(), m_incremental.end(), word_t { 0 });
            m_inc_last = 0;
            m_inc_first = std::numeric_limits<std::size_t>::max();
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

template auto merge_sorter::sort_impl<float>(detail::flat_fitness<float> const&, double) const -> fronts;
template auto merge_sorter::sort_impl<double>(detail::flat_fitness<double> const&, double) const -> fronts;

} // namespace ndsort
