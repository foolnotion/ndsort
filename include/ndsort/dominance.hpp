#pragma once

#include <cmath>
#include <concepts>
#include <cstdint>
#include <iterator>
#include <ranges>

namespace ndsort {

enum class dominance : uint8_t { equal = 0, left = 1, right = 2, none = 3 };

// Epsilon-aware less-than for floating-point scalars.
struct less {
    template<std::floating_point T>
    [[nodiscard]] auto operator()(T a, T b, T eps = T{0}) const noexcept -> bool
    {
        return a < b && (b - a) > eps;
    }
};

// Epsilon-aware equality for floating-point scalars.
struct equal_to {
    template<std::floating_point T>
    [[nodiscard]] auto operator()(T a, T b, T eps = T{0}) const noexcept -> bool
    {
        return std::abs(a - b) <= eps;
    }
};

// Pareto dominance comparison over two iterator ranges.
// Assumes minimisation in every objective.
// Returns dominance::left  if [first1,last1) dominates [first2,last2)
//         dominance::right if [first2,last2) dominates [first1,last1)
//         dominance::equal if identical within eps
//         dominance::none  if neither dominates the other
struct pareto_compare {
    template<std::forward_iterator I1, std::forward_iterator I2>
    [[nodiscard]] auto operator()(I1 first1, I1 last1, I2 first2, I2 last2,
                                  double eps = 0.0) const noexcept -> dominance
    {
        less cmp;
        uint8_t lhs_better{0};
        uint8_t rhs_better{0};
        for (; first1 != last1 && first2 != last2; ++first1, ++first2) {
            auto const a = static_cast<double>(*first1);
            auto const b = static_cast<double>(*first2);
            lhs_better |= static_cast<uint8_t>(cmp(a, b, eps));
            rhs_better |= static_cast<uint8_t>(cmp(b, a, eps));
        }
        return static_cast<dominance>(lhs_better | static_cast<uint8_t>(rhs_better << 1U));
    }

    template<std::ranges::forward_range R1, std::ranges::forward_range R2>
    [[nodiscard]] auto operator()(R1&& r1, R2&& r2, double eps = 0.0) const noexcept -> dominance
    {
        return (*this)(std::ranges::begin(r1), std::ranges::end(r1),
                       std::ranges::begin(r2), std::ranges::end(r2), eps);
    }
};

} // namespace ndsort
