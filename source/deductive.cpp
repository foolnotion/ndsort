// SPDX-License-Identifier: MIT

#include "ndsort/deductive.hpp"
#include "ndsort/dominance.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace ndsort {
namespace {

    template <int M, typename T>
    auto sort_impl_m(detail::flat_fitness<T> const& ff, double eps) -> fronts
    {
        auto const n = ff.n;
        auto const m = M > 0 ? static_cast<std::size_t>(M) : ff.m;

        static constexpr std::size_t digits = std::numeric_limits<uint64_t>::digits;
        auto const nb = n / digits + (n % digits != 0 ? 1UZ : 0UZ);

        std::vector<uint64_t> dominated(nb, 0);
        std::vector<uint64_t> sorted_bits(nb, 0);

        auto set_bit = [](std::vector<uint64_t>& bits, std::size_t i) {
            bits[i / digits] |= (uint64_t { 1 } << (i % digits));
        };
        auto get_bit = [](std::vector<uint64_t> const& bits, std::size_t i) -> bool {
            return (bits[i / digits] >> (i % digits)) & 1U;
        };
        auto dominated_or_sorted = [&](std::size_t i) {
            return get_bit(sorted_bits, i) || get_bit(dominated, i);
        };

        auto compare = [&](std::size_t a, std::size_t b) -> dominance {
            uint8_t lhs { 0 };
            uint8_t rhs { 0 };
            for (std::size_t k = 0; k < m; ++k) {
                auto const va = ff.at(k, a);
                auto const vb = ff.at(k, b);
                lhs |= static_cast<uint8_t>(va < vb && (vb - va) > static_cast<T>(eps));
                rhs |= static_cast<uint8_t>(vb < va && (va - vb) > static_cast<T>(eps));
            }
            return static_cast<dominance>(lhs | static_cast<uint8_t>(rhs << 1U));
        };

        fronts result;
        std::size_t sorted_count { 0 };

        while (sorted_count < n) {
            std::vector<std::size_t> front;
            for (std::size_t i = 0; i < n; ++i) {
                if (dominated_or_sorted(i)) {
                    continue;
                }
                for (std::size_t j = i + 1; j < n; ++j) {
                    if (dominated_or_sorted(j)) {
                        continue;
                    }
                    auto const dom = compare(i, j);
                    if (dom == dominance::right) {
                        set_bit(dominated, i);
                    }
                    if (dom == dominance::left) {
                        set_bit(dominated, j);
                    }
                    if (get_bit(dominated, i)) {
                        break;
                    }
                }
                if (!get_bit(dominated, i)) {
                    front.push_back(i);
                    set_bit(sorted_bits, i);
                }
            }
            std::fill(dominated.begin(), dominated.end(), uint64_t { 0 });
            sorted_count += front.size();
            result.push_back(std::move(front));
        }
        return result;
    }

    template <typename T>
    auto sort_impl_dispatch(detail::flat_fitness<T> const& ff, double eps) -> fronts
    {
        return detail::dispatch_on_m(ff, [&](auto mc) {
            return sort_impl_m<mc.value>(ff, eps);
        });
    }

} // namespace

template <typename T>
auto deductive_sorter::sort_impl(detail::flat_fitness<T> const& ff, double eps) const -> fronts
{
    return sort_impl_dispatch(ff, eps);
}

template NDSORT_EXPORT auto deductive_sorter::sort_impl<float>(detail::flat_fitness<float> const&, double) const -> fronts;
template NDSORT_EXPORT auto deductive_sorter::sort_impl<double>(detail::flat_fitness<double> const&, double) const -> fronts;

} // namespace ndsort
