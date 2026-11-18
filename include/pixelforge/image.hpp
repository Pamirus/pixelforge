#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace pixelforge {

class ImageView;

// An RGB image with 8 bits per channel. Pixels are stored row by row, with the
// channels of each pixel next to each other: RGBRGB...
//
// The pixels live in a std::vector, which already knows how to copy, move and
// free them. So the class declares none of the five special member functions
// and the compiler-generated ones are correct: the Rule of Zero.
class Image {
public:
    static constexpr int channels = 3;

    // An empty image: no pixels, width and height 0.
    Image() = default;

    // A black image. Throws std::invalid_argument unless both sizes are
    // positive, and std::length_error if the size does not fit in memory.
    Image(int width, int height);

    // A copy of the pixels a view shows, like std::string from a
    // std::string_view. Explicit, because it copies.
    explicit Image(ImageView view);

    // A moved-from vector is empty, but the generated move copies the two ints
    // and leaves them as they were. An empty image therefore reports 0 here,
    // so that width * height * channels == size() always holds.
    int width() const noexcept { return pixels_.empty() ? 0 : width_; }
    int height() const noexcept { return pixels_.empty() ? 0 : height_; }
    bool empty() const noexcept { return pixels_.empty(); }

    // Number of bytes in the pixel buffer: width * height * channels.
    std::size_t size() const noexcept { return pixels_.size(); }

    std::uint8_t* data() noexcept { return pixels_.data(); }
    const std::uint8_t* data() const noexcept { return pixels_.data(); }

    // Row y, counting from 0 at the top: width * channels bytes. y is not
    // checked, like the index of operator[]. A const image gives read-only rows.
    std::span<std::uint8_t> row(int y) noexcept
    {
        const auto length = row_length();
        return {data() + y * length, length};
    }
    std::span<const std::uint8_t> row(int y) const noexcept
    {
        const auto length = row_length();
        return {data() + y * length, length};
    }

private:
    std::size_t row_length() const noexcept
    {
        return static_cast<std::size_t>(width()) * channels;
    }

    // pixels_ comes first: the generated copy assignment assigns members in
    // this order, so if copying the pixels throws, the sizes are not yet changed.
    std::vector<std::uint8_t> pixels_;
    int width_ = 0;
    int height_ = 0;
};

} // namespace pixelforge
