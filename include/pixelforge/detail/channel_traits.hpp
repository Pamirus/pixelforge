#pragma once

// Used by the filter templates in filters.cpp and blur.hpp. Not part of the
// interface: callers include filters.hpp or blur.hpp.

#include <cstdint>

namespace pixelforge::detail {

// What differs between the two channel types, in one place. The filters are
// written once and ask ChannelTraits<T> for these. A class template with a
// specialization per type like this is called a traits class;
// std::numeric_limits is one from the standard library.
template <typename T>
struct ChannelTraits;

// 8-bit channels. The arithmetic is done in int, which holds results such as
// 250 + 20 or 10 - 20 before they are clamped, and an average must be rounded
// to a whole number.
template <>
struct ChannelTraits<std::uint8_t> {
    using Work = int;
    static constexpr Work max = 255;

    static Work from_setting(int value) { return value; }

    // A sum of many channel values, and its rounded mean. 64 bits hold the
    // sum of every channel value in any image that fits in memory.
    using Sum = std::uint64_t;
    static std::uint8_t mean(Sum sum, int count)
    {
        const auto n = static_cast<Sum>(count);
        return static_cast<std::uint8_t>((sum + n / 2) / n);
    }

    static std::uint8_t average(const std::uint8_t* pixel)
    {
        const unsigned sum = unsigned{pixel[0]} + pixel[1] + pixel[2];
        return static_cast<std::uint8_t>((sum + 1) / 3);
    }
};

// float channels from 0.0 to 1.0. A setting on the 0 to 255 scale is divided
// by 255, and nothing has to be rounded.
template <>
struct ChannelTraits<float> {
    using Work = float;
    static constexpr Work max = 1.0f;

    static Work from_setting(int value) { return static_cast<float>(value) / 255.0f; }

    using Sum = float;
    static float mean(Sum sum, int count) { return sum / static_cast<float>(count); }

    static float average(const float* pixel) { return (pixel[0] + pixel[1] + pixel[2]) / 3.0f; }
};

} // namespace pixelforge::detail
