#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>
#include <vector>

namespace pixelforge {

template <typename T>
class BasicImageView;

namespace detail {

// width * height * 3, the number of channel values in an image. Throws
// std::invalid_argument unless both sizes are positive, and std::length_error
// if the product does not fit in a std::size_t.
std::size_t channel_count(int width, int height);

} // namespace detail

// An RGB image whose channels are of type T. Pixels are stored row by row,
// with the channels of each pixel next to each other: RGBRGB...
//
//   Image       (T = std::uint8_t)  channels from 0 to 255, as image files hold them
//   FloatImage  (T = float)         channels from 0.0 to 1.0, for filters that
//                                   need fractions; values outside are allowed
//
// Like std::basic_string and std::string, the template has a longer name and
// the two images have short aliases, so code written for 8-bit images before
// the template existed still compiles.
//
// The pixels live in a std::vector, which already knows how to copy, move and
// free them. So the class declares none of the five special member functions
// and the compiler-generated ones are correct: the Rule of Zero.
template <typename T>
class BasicImage {
    static_assert(std::is_same_v<T, std::uint8_t> || std::is_same_v<T, float>,
                  "pixelforge images have std::uint8_t or float channels");

public:
    using channel_type = T;
    static constexpr int channels = 3;

    // An empty image: no pixels, width and height 0.
    BasicImage() = default;

    // A black image. The size is checked before anything is allocated, and
    // vector(n) sets every value to 0. Throws std::invalid_argument unless both
    // sizes are positive, and std::length_error if the size does not fit in
    // memory.
    BasicImage(int width, int height)
        : pixels_(detail::channel_count(width, height)), width_(width), height_(height)
    {
    }

    // A copy of the pixels a view shows, like std::string from a
    // std::string_view. Explicit, because it copies. Defined in
    // image_view.hpp, after the view class.
    explicit BasicImage(BasicImageView<T> view);

    // A moved-from vector is empty, but the generated move copies the two ints
    // and leaves them as they were. An empty image therefore reports 0 here,
    // so that width * height * channels == size() always holds.
    int width() const noexcept { return pixels_.empty() ? 0 : width_; }
    int height() const noexcept { return pixels_.empty() ? 0 : height_; }
    bool empty() const noexcept { return pixels_.empty(); }

    // Number of channel values: width * height * channels. For an 8-bit image
    // that is also the number of bytes.
    std::size_t size() const noexcept { return pixels_.size(); }

    T* data() noexcept { return pixels_.data(); }
    const T* data() const noexcept { return pixels_.data(); }

    // Row y, counting from 0 at the top: width * channels values. y is not
    // checked, like the index of operator[]. A const image gives read-only rows.
    std::span<T> row(int y) noexcept
    {
        const auto length = row_length();
        return {data() + y * length, length};
    }
    std::span<const T> row(int y) const noexcept
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
    std::vector<T> pixels_;
    int width_ = 0;
    int height_ = 0;
};

using Image = BasicImage<std::uint8_t>;
using FloatImage = BasicImage<float>;

// Channel value v becomes v / 255, so 0 to 255 becomes 0.0 to 1.0.
[[nodiscard]] FloatImage to_float(const Image& image);

// Channel value v becomes v * 255, rounded to the nearest whole number.
// Values below 0.0 become 0, values above 1.0 become 255, and NaN becomes 0.
[[nodiscard]] Image to_uint8(const FloatImage& image);

} // namespace pixelforge
