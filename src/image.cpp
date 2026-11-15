#include "pixelforge/image.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>

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

// The size is checked before anything is allocated. The () after new[] sets
// every byte to 0, so a new image is black.
Image::Image(int width, int height)
    : width_(width), height_(height), pixels_(new std::uint8_t[checked_size(width, height)]())
{
}

Image::~Image()
{
    delete[] pixels_;
}

Image::Image(const Image& other)
    : width_(other.width_),
      height_(other.height_),
      pixels_(other.pixels_ != nullptr ? new std::uint8_t[other.size()] : nullptr)
{
    std::copy_n(other.pixels_, other.size(), pixels_);
}

// Copy into a temporary first, then swap. If the allocation throws, *this is
// unchanged. The old pixels are freed when the temporary goes out of scope.
Image& Image::operator=(const Image& other)
{
    if (this != &other) {
        Image copy(other);
        swap(*this, copy);
    }
    return *this;
}

Image::Image(Image&& other) noexcept
    : width_(std::exchange(other.width_, 0)),
      height_(std::exchange(other.height_, 0)),
      pixels_(std::exchange(other.pixels_, nullptr))
{
}

Image& Image::operator=(Image&& other) noexcept
{
    if (this != &other) {
        delete[] pixels_;
        width_ = std::exchange(other.width_, 0);
        height_ = std::exchange(other.height_, 0);
        pixels_ = std::exchange(other.pixels_, nullptr);
    }
    return *this;
}

std::size_t Image::size() const noexcept
{
    return static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_) * channels;
}

void swap(Image& a, Image& b) noexcept
{
    using std::swap;
    swap(a.width_, b.width_);
    swap(a.height_, b.height_);
    swap(a.pixels_, b.pixels_);
}

} // namespace pixelforge
