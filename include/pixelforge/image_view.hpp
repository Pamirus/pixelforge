#pragma once

#include "pixelforge/image.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>

namespace pixelforge {

// A rectangle of pixels inside a BasicImage<T>, without a copy of them. Like
// std::span, a view does not own its pixels: it must not outlive the image or
// be used after the image gets new pixels, and writing through it changes the
// image.
//
// A crop keeps the row length of the whole image as its stride, so its rows
// are not next to each other in memory unless it spans the full width.
//
// ImageView and FloatImageView are the views of Image and FloatImage.
template <typename T>
class BasicImageView {
public:
    static constexpr int channels = BasicImage<T>::channels;

    // An empty view: no pixels, width and height 0.
    BasicImageView() = default;

    // The whole image. Implicit, like std::span from a std::vector, so an
    // image can be passed where a view is expected. Only a non-const lvalue is
    // accepted: a view of a temporary would point at freed pixels after the
    // statement, and a const image must not be changed through a view.
    BasicImageView(BasicImage<T>& image) noexcept
        : BasicImageView(image.data(), image.width(), image.height(),
                         std::ptrdiff_t{image.width()} * channels)
    {
    }

    int width() const noexcept { return width_; }
    int height() const noexcept { return height_; }
    bool empty() const noexcept { return width_ == 0 || height_ == 0; }

    // Channel values (bytes, for an 8-bit image) from the start of one row to
    // the start of the next.
    std::ptrdiff_t stride() const noexcept { return stride_; }

    // The first of the three channel values of the pixel in column x and row
    // y, counted from the view's top-left corner. Not checked, like operator[].
    T* pixel(int x, int y) const noexcept
    {
        return data_ + y * stride_ + std::ptrdiff_t{x} * channels;
    }

    // Row y of the view, counting from 0 at its top: width * channels values.
    // Not checked. Like a const std::span, a const view still gives writable
    // rows; only a const image is read-only.
    std::span<T> row(int y) const noexcept
    {
        return {pixel(0, y), static_cast<std::size_t>(width_) * channels};
    }

    // The width × height rectangle of this view whose top-left corner is pixel
    // (x, y). Nothing is copied. Throws std::invalid_argument unless both
    // sizes are positive, and std::out_of_range if the rectangle does not fit
    // inside this view.
    [[nodiscard]] BasicImageView crop(int x, int y, int width, int height) const
    {
        if (width <= 0 || height <= 0) {
            throw std::invalid_argument("crop width and height must be positive");
        }
        // Subtracting instead of comparing x + width with width_, which could overflow.
        if (x < 0 || y < 0 || x > width_ - width || y > height_ - height) {
            throw std::out_of_range("crop rectangle does not fit inside the view");
        }
        return BasicImageView(pixel(x, y), width, height, stride_);
    }

private:
    BasicImageView(T* data, int width, int height, std::ptrdiff_t stride) noexcept
        : data_(data), width_(width), height_(height), stride_(stride)
    {
    }

    T* data_ = nullptr;
    int width_ = 0;
    int height_ = 0;
    std::ptrdiff_t stride_ = 0;
};

using ImageView = BasicImageView<std::uint8_t>;
using FloatImageView = BasicImageView<float>;

// Declared in image.hpp. The rows of a crop are not next to each other in
// memory, so they are copied one at a time.
template <typename T>
BasicImage<T>::BasicImage(BasicImageView<T> view)
    : pixels_(view.empty() ? 0 : detail::channel_count(view.width(), view.height())),
      width_(view.width()),
      height_(view.height())
{
    for (int y = 0; y < height_; ++y) {
        std::ranges::copy(view.row(y), row(y).begin());
    }
}

} // namespace pixelforge
