#pragma once

#include "pixelforge/image.hpp"

#include <cstddef>
#include <cstdint>

namespace pixelforge {

// A rectangle of pixels inside an Image, without a copy of them. Like
// std::span, a view does not own its pixels: it must not outlive the image or
// be used after the image gets new pixels, and writing through it changes the
// image.
//
// A crop keeps the row length of the whole image as its stride, so its rows
// are not next to each other in memory unless it spans the full width.
class ImageView {
public:
    static constexpr int channels = Image::channels;

    // An empty view: no pixels, width and height 0.
    ImageView() = default;

    // The whole image. Implicit, like std::span from a std::vector, so an
    // Image can be passed where a view is expected. Only a non-const lvalue is
    // accepted: a view of a temporary would point at freed pixels after the
    // statement, and a const image must not be changed through a view.
    ImageView(Image& image) noexcept
        : ImageView(image.data(), image.width(), image.height(),
                    std::ptrdiff_t{image.width()} * channels)
    {
    }

    int width() const noexcept { return width_; }
    int height() const noexcept { return height_; }
    bool empty() const noexcept { return width_ == 0 || height_ == 0; }

    // Bytes from the start of one row to the start of the next.
    std::ptrdiff_t stride() const noexcept { return stride_; }

    // The first of the three channel bytes of the pixel in column x and row y,
    // counted from the view's top-left corner. Not checked, like operator[].
    std::uint8_t* pixel(int x, int y) const noexcept
    {
        return data_ + y * stride_ + std::ptrdiff_t{x} * channels;
    }

    // The width × height rectangle of this view whose top-left corner is pixel
    // (x, y). Nothing is copied. Throws std::invalid_argument unless both
    // sizes are positive, and std::out_of_range if the rectangle does not fit
    // inside this view.
    [[nodiscard]] ImageView crop(int x, int y, int width, int height) const;

private:
    ImageView(std::uint8_t* data, int width, int height, std::ptrdiff_t stride) noexcept
        : data_(data), width_(width), height_(height), stride_(stride)
    {
    }

    std::uint8_t* data_ = nullptr;
    int width_ = 0;
    int height_ = 0;
    std::ptrdiff_t stride_ = 0;
};

} // namespace pixelforge
