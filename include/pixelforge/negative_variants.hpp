#pragma once

// One filter, the negative, written three ways. They do the same work, and
// stage v9 asks whether the nicer-looking ones cost anything:
//
//   raw loop        what filters.cpp does
//   STL algorithm   std::transform with a lambda: "what to do to each value",
//                   with the loop left to the library
//   ranges          a lazy view that maps each value, copied back into the image
//
// They are templates in a header so that the compiler sees the loop and the
// lambda together and can inline one into the other. That is where
// "zero-cost" comes from; a call through a pointer it could not see through
// would be a different experiment (see the v7 benchmarks).

#include "pixelforge/detail/channel_traits.hpp"
#include "pixelforge/image.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <ranges>
#include <span>

namespace pixelforge::variants {

// 1. The loop written out.
template <typename T>
void negative_raw_loop(BasicImage<T>& image)
{
    T* p = image.data();
    const std::size_t size = image.size();
    for (std::size_t i = 0; i < size; ++i) {
        p[i] = static_cast<T>(detail::ChannelTraits<T>::max - p[i]);
    }
}

// 2. std::transform: the lambda is the filter, the algorithm is the loop. The
// output iterator is the input iterator, which transform allows: each value
// is read once and then written to the same place.
template <typename T>
void negative_transform(BasicImage<T>& image)
{
    const std::span<T> values(image.data(), image.size());
    std::transform(values.begin(), values.end(), values.begin(), [](T value) {
        return static_cast<T>(detail::ChannelTraits<T>::max - value);
    });
}

// 3. Ranges: `values | std::views::transform(f)` is a lazy sequence that
// computes f(value) when it is read, and std::ranges::copy reads it and
// writes the results back. The view does not own or copy anything.
template <typename T>
void negative_ranges(BasicImage<T>& image)
{
    const std::span<T> values(image.data(), image.size());
    auto flipped = values | std::views::transform([](T value) {
                       return static_cast<T>(detail::ChannelTraits<T>::max - value);
                   });
    std::ranges::copy(flipped, values.begin());
}

} // namespace pixelforge::variants
