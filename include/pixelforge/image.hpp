#pragma once

#include <cstddef>
#include <cstdint>

namespace pixelforge {

// An RGB image with 8 bits per channel. Pixels are stored row by row, with the
// channels of each pixel next to each other: RGBRGB...
//
// The image owns its pixels. It allocates them with new[] and frees them in the
// destructor, so it also has to say what copying and moving mean. That makes
// five special member functions written by hand: the Rule of Five.
class Image {
public:
    static constexpr int channels = 3;

    // An empty image: no pixels, width and height 0.
    Image() noexcept = default;

    // A black image. Throws std::invalid_argument unless both sizes are
    // positive, and std::length_error if the size does not fit in memory.
    Image(int width, int height);

    ~Image();

    // Copying allocates new pixels and copies them (a deep copy).
    Image(const Image& other);
    Image& operator=(const Image& other);

    // Moving takes the other image's pixels and leaves it empty.
    Image(Image&& other) noexcept;
    Image& operator=(Image&& other) noexcept;

    int width() const noexcept { return width_; }
    int height() const noexcept { return height_; }
    bool empty() const noexcept { return pixels_ == nullptr; }

    // Number of bytes in the pixel buffer: width * height * channels.
    std::size_t size() const noexcept;

    std::uint8_t* data() noexcept { return pixels_; }
    const std::uint8_t* data() const noexcept { return pixels_; }

    friend void swap(Image& a, Image& b) noexcept;

private:
    int width_ = 0;
    int height_ = 0;
    std::uint8_t* pixels_ = nullptr;
};

} // namespace pixelforge
