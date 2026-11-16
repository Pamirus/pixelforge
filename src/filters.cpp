#include "pixelforge/filters.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace pixelforge {

namespace {

// The rounded average of a pixel's three channels.
std::uint8_t gray_level(const std::uint8_t* pixel)
{
    const unsigned sum = unsigned{pixel[0]} + pixel[1] + pixel[2];
    return static_cast<std::uint8_t>((sum + 1) / 3);
}

} // namespace

void grayscale(Image& image)
{
    std::uint8_t* p = image.data();
    const std::size_t size = image.size();
    for (std::size_t i = 0; i < size; i += Image::channels) {
        const std::uint8_t gray = gray_level(p + i);
        p[i] = gray;
        p[i + 1] = gray;
        p[i + 2] = gray;
    }
}

void brightness(Image& image, int amount)
{
    // Larger steps change nothing more, and limiting the amount keeps
    // p[i] + amount far away from int overflow.
    amount = std::clamp(amount, -255, 255);

    std::uint8_t* p = image.data();
    const std::size_t size = image.size();
    for (std::size_t i = 0; i < size; ++i) {
        p[i] = static_cast<std::uint8_t>(std::clamp(p[i] + amount, 0, 255));
    }
}

void negative(Image& image)
{
    std::uint8_t* p = image.data();
    const std::size_t size = image.size();
    for (std::size_t i = 0; i < size; ++i) {
        p[i] = static_cast<std::uint8_t>(255 - p[i]);
    }
}

void threshold(Image& image, int level)
{
    std::uint8_t* p = image.data();
    const std::size_t size = image.size();
    for (std::size_t i = 0; i < size; i += Image::channels) {
        const std::uint8_t value = gray_level(p + i) >= level ? 255 : 0;
        p[i] = value;
        p[i + 1] = value;
        p[i + 2] = value;
    }
}

} // namespace pixelforge
