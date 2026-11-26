#include "pixelforge/parallel.hpp"

#include "pixelforge/detail/channel_traits.hpp"
#include "pixelforge/filters.hpp"

#include <algorithm>
#include <cstdint>
#include <stdexcept>

namespace pixelforge {

using detail::ChannelTraits;

void brightness_parallel(Image& image, int amount, unsigned threads)
{
    std::uint8_t* const p = image.data();
    parallel_chunks(image.size(), threads, [=](std::size_t begin, std::size_t end) {
        brighten_bytes(p + begin, end - begin, amount);
    });
}

void grayscale_parallel(Image& image, unsigned threads)
{
    std::uint8_t* const p = image.data();
    const std::size_t pixels = image.size() / Image::channels;
    // The pieces are counted in pixels, so no piece cuts a pixel in two.
    parallel_chunks(pixels, threads, [=](std::size_t begin, std::size_t end) {
        for (std::size_t n = begin; n < end; ++n) {
            std::uint8_t* const pixel = p + n * Image::channels;
            const std::uint8_t gray = ChannelTraits<std::uint8_t>::average(pixel);
            pixel[0] = gray;
            pixel[1] = gray;
            pixel[2] = gray;
        }
    });
}

void box_blur_parallel(Image& image, int radius, unsigned threads)
{
    if (radius < 0) {
        throw std::invalid_argument("blur radius must not be negative");
    }
    if (radius == 0 || image.empty()) {
        return;
    }
    // Every thread reads this unchanged copy and writes its own rows of image.
    const Image source = image;
    Image* const out = &image;
    parallel_chunks(static_cast<std::size_t>(image.height()), threads,
                    [&source, out, radius](std::size_t begin, std::size_t end) {
                        box_blur_clipped_rows(source, *out, radius, static_cast<int>(begin),
                                              static_cast<int>(end));
                    });
}

} // namespace pixelforge
