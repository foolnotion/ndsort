// SPDX-License-Identifier: MIT

#include "ndsort/hierarchical_sort.hpp"

#include <algorithm>
#include <cstddef>
#include <deque>
#include <numeric>
#include <vector>

namespace ndsort {
namespace {

template<int M>
auto sort_impl_m(detail::flat_fitness const& ff, double eps) -> fronts
{
    auto const n = ff.n;
    auto const m = M > 0 ? static_cast<std::size_t>(M) : ff.m;

    // a weakly ε-dominates b if a[k] <= b[k] + eps for all k.
    auto dominates = [&](std::size_t a, std::size_t b) -> bool {
        for (std::size_t k = 0; k < m; ++k) {
            if (ff.at(k, a) > ff.at(k, b) + eps) { return false; }
        }
        return true;
    };

    std::deque<std::size_t> queue(n);
    std::iota(queue.begin(), queue.end(), std::size_t{0});
    std::deque<std::size_t> dominated_queue;

    fronts result;
    while (!queue.empty()) {
        std::vector<std::size_t> front;
        while (!queue.empty()) {
            auto q1 = queue.front();
            queue.pop_front();
            front.push_back(q1);

            std::size_t non_dominated_count{0};
            while (queue.size() > non_dominated_count) {
                auto qj = queue.front();
                queue.pop_front();
                if (!dominates(q1, qj)) {
                    queue.push_back(qj);
                    ++non_dominated_count;
                } else {
                    dominated_queue.push_back(qj);
                }
            }
        }
        std::stable_sort(dominated_queue.begin(), dominated_queue.end());
        std::swap(dominated_queue, queue);
        dominated_queue.clear();
        result.push_back(std::move(front));
    }
    return result;
}

} // namespace

auto hierarchical_sorter::sort_impl(detail::flat_fitness const& ff, double eps) const -> fronts
{
    return detail::dispatch_on_m(ff, [&](auto mc) {
        return sort_impl_m<mc.value>(ff, eps);
    });
}

} // namespace ndsort
