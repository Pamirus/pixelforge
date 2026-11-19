#include "pixelforge/filters.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <span>
#include <string>

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

void flip_horizontal(ImageView view)
{
    for (int y = 0; y < view.height(); ++y) {
        for (int left = 0, right = view.width() - 1; left < right; ++left, --right) {
            std::swap_ranges(view.pixel(left, y), view.pixel(left, y) + Image::channels,
                             view.pixel(right, y));
        }
    }
}

void flip_vertical(ImageView view)
{
    for (int top = 0, bottom = view.height() - 1; top < bottom; ++top, --bottom) {
        std::ranges::swap_ranges(view.row(top), view.row(bottom));
    }
}

// Column x of the image, read from the bottom up, becomes row x of the result.
Image rotate_90(const Image& image)
{
    if (image.empty()) {
        return Image();
    }
    Image rotated(image.height(), image.width());
    for (int x = 0; x < image.width(); ++x) {
        const std::span<std::uint8_t> out = rotated.row(x);
        const std::size_t column = static_cast<std::size_t>(x) * Image::channels;
        for (int y = 0; y < image.height(); ++y) {
            const auto pixel = image.row(image.height() - 1 - y).subspan(column, Image::channels);
            std::ranges::copy(pixel, out.begin() + y * Image::channels);
        }
    }
    return rotated;
}

void Grayscale::apply(Image& image) const
{
    grayscale(image);
}

std::string Grayscale::description() const
{
    return "grayscale";
}

void Brightness::apply(Image& image) const
{
    brightness(image, amount_);
}

std::string Brightness::description() const
{
    return std::format("brightness {}", amount_);
}

void Negative::apply(Image& image) const
{
    negative(image);
}

std::string Negative::description() const
{
    return "negative";
}

void Threshold::apply(Image& image) const
{
    threshold(image, level_);
}

std::string Threshold::description() const
{
    return std::format("threshold {}", level_);
}

void FlipHorizontal::apply(Image& image) const
{
    flip_horizontal(image);
}

std::string FlipHorizontal::description() const
{
    return "flip horizontal";
}

void FlipVertical::apply(Image& image) const
{
    flip_vertical(image);
}

std::string FlipVertical::description() const
{
    return "flip vertical";
}

// The rotated image is a temporary, so assigning it moves its pixels in.
void Rotate90::apply(Image& image) const
{
    image = rotate_90(image);
}

std::string Rotate90::description() const
{
    return "rotate 90";
}

} // namespace pixelforge
