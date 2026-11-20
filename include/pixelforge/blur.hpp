#pragma once

#include "pixelforge/detail/channel_traits.hpp"
#include "pixelforge/image.hpp"

#include <cstddef>
#include <span>

namespace pixelforge {

// The box blur of filters.hpp with the window size as a template parameter:
// Blur<3>{}(image) gives the same result as box_blur(image, 1), and
// Blur<5>{}(image) the same as box_blur(image, 2).
//
// Because the size is part of the type, the compiler knows the loop bounds in
// every Blur<Size> it generates and can unroll the loops over the window. The
// price is that the size must be known when the program is compiled, and
// every size used is compiled separately. The loops are otherwise the same as
// in box_blur, border checks included, so the two can be compared.
//
// Unlike the other filters, this one is defined in the header: each file that
// uses Blur<7> needs the code to generate it, and filters.cpp cannot know
// which sizes will be used.
template <int Size>
struct Blur {
    static_assert(Size >= 3 && Size % 2 == 1,
                  "the blur window must have an odd size of at least 3, so that it has a middle");

    static constexpr int radius = Size / 2;

    template <typename T>
    void operator()(BasicImage<T>& image) const
    {
        if (image.empty()) {
            return;
        }
        using Traits = detail::ChannelTraits<T>;
        constexpr int channels = BasicImage<T>::channels;
        const int width = image.width();
        const int height = image.height();

        // The original values of the neighbours are needed after some of them
        // are overwritten, so the blur reads from a copy.
        const BasicImage<T> source = image;

        for (int y = 0; y < height; ++y) {
            const std::span<T> out = image.row(y);
            for (int x = 0; x < width; ++x) {
                typename Traits::Sum sum[channels] = {};
                int count = 0;
                for (int dy = -radius; dy <= radius; ++dy) {
                    for (int dx = -radius; dx <= radius; ++dx) {
                        const int sx = x + dx;
                        const int sy = y + dy;
                        if (sx < 0 || sx >= width || sy < 0 || sy >= height) {
                            continue;
                        }
                        const T* pixel = source.row(sy).data() + sx * channels;
                        for (int c = 0; c < channels; ++c) {
                            sum[c] += pixel[c];
                        }
                        ++count;
                    }
                }
                for (int c = 0; c < channels; ++c) {
                    out[static_cast<std::size_t>(x) * channels + c] = Traits::mean(sum[c], count);
                }
            }
        }
    }
};

} // namespace pixelforge
