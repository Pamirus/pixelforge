#include "pixelforge/planar.hpp"

#include "pixelforge/detail/channel_traits.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace pixelforge {

namespace {

void check_channel(int channel)
{
    if (channel < 0 || channel >= Image::channels) {
        throw std::invalid_argument("channel must be 0 (red), 1 (green) or 2 (blue)");
    }
}

std::uint8_t brightened(std::uint8_t value, int step)
{
    return static_cast<std::uint8_t>(std::clamp(value + step, 0, 255));
}

} // namespace

PlanarImage::PlanarImage(int width, int height)
    : data_(detail::channel_count(width, height)), width_(width), height_(height)
{
}

PlanarImage to_planar(const Image& image)
{
    if (image.empty()) {
        return PlanarImage();
    }
    PlanarImage planar(image.width(), image.height());
    const std::uint8_t* in = image.data();
    std::uint8_t* red = planar.plane(0).data();
    std::uint8_t* green = planar.plane(1).data();
    std::uint8_t* blue = planar.plane(2).data();
    const std::size_t pixels = planar.plane(0).size();
    for (std::size_t i = 0; i < pixels; ++i) {
        red[i] = in[3 * i];
        green[i] = in[3 * i + 1];
        blue[i] = in[3 * i + 2];
    }
    return planar;
}

Image to_interleaved(const PlanarImage& image)
{
    if (image.empty()) {
        return Image();
    }
    Image result(image.width(), image.height());
    std::uint8_t* out = result.data();
    const std::uint8_t* red = image.plane(0).data();
    const std::uint8_t* green = image.plane(1).data();
    const std::uint8_t* blue = image.plane(2).data();
    const std::size_t pixels = image.plane(0).size();
    for (std::size_t i = 0; i < pixels; ++i) {
        out[3 * i] = red[i];
        out[3 * i + 1] = green[i];
        out[3 * i + 2] = blue[i];
    }
    return result;
}

// The three planes are read side by side, one value from each per pixel.
void grayscale(PlanarImage& image)
{
    if (image.empty()) {
        return;
    }
    std::uint8_t* red = image.plane(0).data();
    std::uint8_t* green = image.plane(1).data();
    std::uint8_t* blue = image.plane(2).data();
    const std::size_t pixels = image.plane(0).size();
    for (std::size_t i = 0; i < pixels; ++i) {
        const std::uint8_t gray =
            detail::ChannelTraits<std::uint8_t>::average(red[i], green[i], blue[i]);
        red[i] = gray;
        green[i] = gray;
        blue[i] = gray;
    }
}

// Every value gets the same treatment, so the planes can be handled as one block.
void brightness(PlanarImage& image, int amount)
{
    const int step = std::clamp(amount, -255, 255);
    std::uint8_t* p = image.data();
    for (std::size_t i = 0, n = image.size(); i < n; ++i) {
        p[i] = brightened(p[i], step);
    }
}

void brighten_channel(PlanarImage& image, int channel, int amount)
{
    check_channel(channel);
    const int step = std::clamp(amount, -255, 255);
    for (std::uint8_t& value : image.plane(channel)) {
        value = brightened(value, step);
    }
}

void brighten_channel(Image& image, int channel, int amount)
{
    check_channel(channel);
    const int step = std::clamp(amount, -255, 255);
    // Counts pixels rather than stepping by 3 through the bytes, for the same
    // reason as grayscale(): a loop with `i += 3` is "uncountable" to Clang and
    // is not vectorized.
    std::uint8_t* const p = image.data() + channel;
    const std::size_t pixels = image.size() / Image::channels;
    for (std::size_t n = 0; n < pixels; ++n) {
        p[n * Image::channels] = brightened(p[n * Image::channels], step);
    }
}

} // namespace pixelforge
