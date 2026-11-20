#pragma once

// Used by the filter templates in filters.cpp and blur.hpp. Not part of the
// interface: callers include filters.hpp or blur.hpp.

#include <cstdint>
#include <type_traits>

namespace pixelforge::detail {

// What differs between the two channel types, in one place. The filters are
// written once and ask ChannelTraits<T> for these. A class template like this
// is called a traits class; std::numeric_limits is one from the standard
// library.
//
// Up to v4 there was one specialization per type. Since v5 there is a single
// template, and each function chooses its 8-bit or float code with
// if constexpr. The branch that does not apply is discarded when the template
// is instantiated. So filters.cpp compiles to the same object file as it did
// with the specializations, byte for byte, and a branch may hold code that would not compile for the other
// type: unsigned{pixel[0]} below is an error for a float pixel.
//
// 8-bit channels are worked on in int, which holds results such as 250 + 20 or
// 10 - 20 before they are clamped, and averages are rounded to whole numbers.
// float channels go from 0.0 to 1.0: a setting on the 0 to 255 scale is
// divided by 255, and nothing has to be rounded.
template <typename T>
struct ChannelTraits {
    static_assert(std::is_same_v<T, std::uint8_t> || std::is_same_v<T, float>,
                  "channels are std::uint8_t or float");

    static constexpr bool is_8_bit = std::is_same_v<T, std::uint8_t>;

    using Work = std::conditional_t<is_8_bit, int, float>;
    static constexpr Work max = is_8_bit ? Work{255} : Work{1};

    // A sum of many channel values. 64 bits hold the sum of every channel
    // value in any 8-bit image that fits in memory.
    using Sum = std::conditional_t<is_8_bit, std::uint64_t, float>;

    static Work from_setting(int value)
    {
        if constexpr (is_8_bit) {
            return value;
        } else {
            return static_cast<float>(value) / 255.0f;
        }
    }

    // The mean of count channel values that add up to sum.
    static T mean(Sum sum, int count)
    {
        if constexpr (is_8_bit) {
            const auto n = static_cast<Sum>(count);
            return static_cast<T>((sum + n / 2) / n);
        } else {
            return sum / static_cast<float>(count);
        }
    }

    // The average of the three channels of a pixel.
    static T average(const T* pixel)
    {
        if constexpr (is_8_bit) {
            const unsigned sum = unsigned{pixel[0]} + pixel[1] + pixel[2];
            return static_cast<T>((sum + 1) / 3);
        } else {
            return (pixel[0] + pixel[1] + pixel[2]) / 3.0f;
        }
    }
};

} // namespace pixelforge::detail
