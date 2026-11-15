#include "pixelforge/image.hpp"

#include <limits>
#include <stdexcept>

namespace pixelforge {

namespace {

std::size_t checked_size(int width, int height)
{
    if (width <= 0 || height <= 0) {
        throw std::invalid_argument("image width and height must be positive");
    }
    const auto w = static_cast<std::size_t>(width);
    const auto h = static_cast<std::size_t>(height);
    if (w > std::numeric_limits<std::size_t>::max() / Image::channels / h) {
        throw std::length_error("image is too large");
    }
    return w * h * Image::channels;
}

} // namespace

// The size is checked before anything is allocated. vector(n) sets every byte
// to 0, so a new image is black.
Image::Image(int width, int height)
    : pixels_(checked_size(width, height)), width_(width), height_(height)
{
}

} // namespace pixelforge
