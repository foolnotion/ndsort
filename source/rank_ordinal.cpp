// SPDX-License-Identifier: MIT

#include "ndsort/rank_ordinal.hpp"

#include <eve/module/algo.hpp>

#include <algorithm>
#include <cstddef>
#include <numeric>
#include <span>
#include <vector>

namespace ndsort {
namespace {

    // ord_rank is stored individual-major: ord_rank[i * m + k] = ordinal rank of individual i
    // in objective k's sort order.  This makes each individual's m-element rank slice
    // contiguous in memory, enabling EVE SIMD for the dominance check.
    auto ord_dominated(std::span<int const> ri, std::span<int const> rj) -> bool
    {
        return eve::algo::all_of(
            eve::views::zip(ri, rj),
            [](auto t) { return kumi::apply(std::less {}, t); });
    }

    template <typename T>
    auto rank_ordinal_impl(detail::flat_fitness<T> const& ff) -> fronts
    {
        auto const n = ff.n;
        auto const m = ff.m;
        auto const ni = static_cast<int>(n);
        auto const mi = static_cast<int>(m);

        // perm[k*n + pos]: individual at position pos in objective k's sort order.
        // Stored objective-major so that each column is a contiguous block for sorting.
        std::vector<int> perm(m * n);

        // ord_rank[i*m + k]: ordinal rank of individual i in objective k.
        // Stored individual-major so that each row is contiguous for EVE zip.
        std::vector<int> ord_rank(n * m);

        // Objective 0: flat_fitness is already lex-sorted so objective 0 is in ascending order.
        for (int i = 0; i < ni; ++i) {
            perm[i] = i;
        }
        for (int i = 0; i < ni; ++i) {
            ord_rank[static_cast<std::size_t>(i) * m] = i;
        }

        // Objectives 1..m-1: stable-sort from the previous objective's permutation so that
        // ties on objective k break in the order established by objective k-1.
        for (std::size_t k = 1; k < m; ++k) {
            int* pk = perm.data() + k * n;
            int* pk_prev = perm.data() + (k - 1) * n;
            std::copy_n(pk_prev, ni, pk);
            std::stable_sort(pk, pk + ni, [&](int a, int b) {
                return ff.at(k, static_cast<std::size_t>(a)) < ff.at(k, static_cast<std::size_t>(b));
            });
            for (int j = 0; j < ni; ++j) {
                ord_rank[static_cast<std::size_t>(pk[j]) * m + k] = j;
            }
        }

        // For each individual: maximum ordinal rank over all objectives and which objective
        // achieves it.  The candidate set for dominance checks is bounded by this max rank.
        std::vector<int> maxp(n), maxc(n);
        for (std::size_t i = 0; i < n; ++i) {
            int const* ri = ord_rank.data() + i * m;
            int mp = ri[0], mc = 0;
            for (int k = 1; k < mi; ++k) {
                if (ri[k] > mp) {
                    mp = ri[k];
                    mc = k;
                }
            }
            maxp[i] = mp;
            maxc[i] = mc;
        }

        // Assign Pareto ranks.  Iterate individuals in objective-0 order (ascending fitness).
        // For individual i only candidates j that appear strictly after position maxp[i] in
        // objective maxc[i]'s sorted order can possibly be dominated by i.
        std::vector<int> rank(n, 0);
        int const* perm0 = perm.data();

        for (int pos = 0; pos < ni - 1; ++pos) {
            auto const i = static_cast<std::size_t>(perm0[pos]);
            if (maxp[i] == ni - 1) {
                continue;
            }

            int const mc = maxc[i];
            int const start = maxp[i] + 1;
            int const* pmc = perm.data() + mc * n;

            std::span<int const> ri { ord_rank.data() + i * m, m };

            for (int q = start; q < ni; ++q) {
                auto const j = static_cast<std::size_t>(pmc[q]);
                if (rank[i] != rank[j]) {
                    continue;
                }
                std::span<int const> rj { ord_rank.data() + j * m, m };
                if (ord_dominated(ri, rj)) {
                    ++rank[j];
                }
            }
        }

        auto const rmax = static_cast<std::size_t>(*std::max_element(rank.begin(), rank.end()));
        fronts result(rmax + 1);
        for (std::size_t i = 0; i < n; ++i) {
            result[static_cast<std::size_t>(rank[i])].push_back(i);
        }
        return result;
    }

} // namespace

template <typename T>
auto rank_ordinal_sorter::sort_impl(detail::flat_fitness<T> const& ff, double) const -> fronts
{
    return rank_ordinal_impl(ff);
}

template NDSORT_EXPORT auto rank_ordinal_sorter::sort_impl<float>(detail::flat_fitness<float> const&, double) const -> fronts;
template NDSORT_EXPORT auto rank_ordinal_sorter::sort_impl<double>(detail::flat_fitness<double> const&, double) const -> fronts;

} // namespace ndsort
