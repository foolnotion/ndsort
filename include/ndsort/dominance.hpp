#pragma once

#include <cmath>
#include <concepts>
#include <cstdint>
#include <iterator>
#include <ranges>

namespace ndsort {

enum class dominance : uint8_t { equal = 0,
    left = 1,
    right = 2,
    none = 3 };

struct less {
    template <std::floating_point T>
    [[nodiscard]] auto operator()(T a, T b, T eps = T { 0 }) const noexcept -> bool
    {
        return a < b && (b - a) > eps;
    }
};

struct equal_to {
    template <std::floating_point T>
    [[nodiscard]] auto operator()(T a, T b, T eps = T { 0 }) const noexcept -> bool
    {
        return std::abs(a - b) <= eps;
    }
};

struct pareto_compare {
    template <std::forward_iterator I1, std::forward_iterator I2>
    [[nodiscard]] auto operator()(I1 first1, I1 last1, I2 first2, I2 last2,
        double eps = 0.0) const noexcept -> dominance
    {
        using val_t = std::common_type_t<typename std::iterator_traits<I1>::value_type,
            typename std::iterator_traits<I2>::value_type,
            double>;
        uint8_t lhs_better { 0 };
        uint8_t rhs_better { 0 };
        for (; first1 != last1 && first2 != last2; ++first1, ++first2) {
            auto const a = static_cast<val_t>(*first1);
            auto const b = static_cast<val_t>(*first2);
            lhs_better |= static_cast<uint8_t>(a < b && (b - a) > static_cast<val_t>(eps));
            rhs_better |= static_cast<uint8_t>(b < a && (a - b) > static_cast<val_t>(eps));
        }
        return static_cast<dominance>(lhs_better | static_cast<uint8_t>(rhs_better << 1U));
    }

    template <std::ranges::forward_range R1, std::ranges::forward_range R2>
    [[nodiscard]] auto operator()(R1&& r1, R2&& r2, double eps = 0.0) const noexcept -> dominance
    {
        return (*this)(std::ranges::begin(r1), std::ranges::end(r1),
            std::ranges::begin(r2), std::ranges::end(r2), eps);
    }
};

} // namespace ndsort
