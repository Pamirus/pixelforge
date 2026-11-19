#include "pixelforge/image.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace pixelforge {

namespace detail {

std::size_t channel_count(int width, int height)
{
    if (width <= 0 || height <= 0) {
        throw std::invalid_argument("image width and height must be positive");
    }
    constexpr std::size_t channels = Image::channels;
    const auto w = static_cast<std::size_t>(width);
    const auto h = static_cast<std::size_t>(height);
    if (w > std::numeric_limits<std::size_t>::max() / channels / h) {
        throw std::length_error("image is too large");
    }
    return w * h * channels;
}

} // namespace detail

FloatImage to_float(const Image& image)
{
    if (image.empty()) {
        return FloatImage();
    }
    FloatImage result(image.width(), image.height());
    std::transform(image.data(), image.data() + image.size(), result.data(),
                   [](std::uint8_t v) { return v / 255.0f; });
    return result;
}

Image to_uint8(const FloatImage& image)
{
    if (image.empty()) {
        return Image();
    }
    Image result(image.width(), image.height());
    std::transform(image.data(), image.data() + image.size(), result.data(), [](float v) {
        // Written so that NaN, for which every comparison is false, gives 0.
        const float clamped = v > 0.0f ? std::min(v, 1.0f) : 0.0f;
        return static_cast<std::uint8_t>(clamped * 255.0f + 0.5f);
    });
    return result;
}

} // namespace pixelforge
