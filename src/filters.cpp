#include "pixelforge/filters.hpp"

#include "pixelforge/detail/channel_traits.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace pixelforge {

using detail::ChannelTraits;

template <typename T>
void grayscale(BasicImage<T>& image)
{
    T* p = image.data();
    const std::size_t size = image.size();
    for (std::size_t i = 0; i < size; i += BasicImage<T>::channels) {
        const T gray = ChannelTraits<T>::average(p + i);
        p[i] = gray;
        p[i + 1] = gray;
        p[i + 2] = gray;
    }
}

// Written in 8-bit arithmetic on purpose. The first version computed
// std::clamp<int>(p[i] + step, 0, 255): correct, but the compiler then widens
// every byte to a 32-bit number, clamps four of them per vector instruction
// and narrows them back. Stage v11 measured that: the filter ran at the same
// speed whether the image was in the cache or not, so the instructions, not
// the memory, were the limit (docs/memory-vs-compute.md).
//
// Here the sum stays a byte. For a positive step, `value + step` wraps around
// past 255 exactly when the true sum is too large, and then the result is
// smaller than value; Clang turns that into one saturating add (uqadd on ARM)
// per 16 bytes. For a negative step the result is max(value, step) - step,
// which became a vector max and a subtraction: two instructions per 16 bytes
// (checked in the assembly of Apple Clang 21; a standalone test function got
// a single uqsub, the library's loop did not).
void brighten_bytes(std::uint8_t* p, std::size_t count, int amount)
{
    amount = std::clamp(amount, -255, 255);
    if (amount >= 0) {
        const auto step = static_cast<std::uint8_t>(amount);
        for (std::size_t i = 0; i < count; ++i) {
            const auto sum = static_cast<std::uint8_t>(p[i] + step);
            p[i] = sum < p[i] ? std::uint8_t{255} : sum;
        }
    } else {
        // max(value, step) - step is value - step, or 0 where value < step.
        // Written as "subtract, then 0 if it wrapped", like the positive
        // case, Clang made it an add, a compare and a mask: three
        // instructions per 16 bytes instead of two.
        const auto step = static_cast<std::uint8_t>(-amount);
        for (std::size_t i = 0; i < count; ++i) {
            p[i] = static_cast<std::uint8_t>(std::max(p[i], step) - step);
        }
    }
}

template <typename T>
void brightness(BasicImage<T>& image, int amount)
{
    if constexpr (ChannelTraits<T>::is_8_bit) {
        brighten_bytes(image.data(), image.size(), amount);
    } else {
        using Traits = ChannelTraits<T>;
        // Larger steps change nothing more.
        const float step = Traits::from_setting(std::clamp(amount, -255, 255));
        T* p = image.data();
        const std::size_t size = image.size();
        for (std::size_t i = 0; i < size; ++i) {
            p[i] = std::clamp(p[i] + step, 0.0f, Traits::max);
        }
    }
}

void brightness_lookup(Image& image, int amount)
{
    const std::array<std::uint8_t, 256> table = brightness_table(amount);
    std::uint8_t* p = image.data();
    const std::size_t size = image.size();
    for (std::size_t i = 0; i < size; ++i) {
        p[i] = table[p[i]];
    }
}

template <typename T>
void negative(BasicImage<T>& image)
{
    T* p = image.data();
    const std::size_t size = image.size();
    for (std::size_t i = 0; i < size; ++i) {
        p[i] = static_cast<T>(ChannelTraits<T>::max - p[i]);
    }
}

template <typename T>
void threshold(BasicImage<T>& image, int level)
{
    using Traits = ChannelTraits<T>;
    const typename Traits::Work limit = Traits::from_setting(level);
    const T white = static_cast<T>(Traits::max);

    T* p = image.data();
    const std::size_t size = image.size();
    for (std::size_t i = 0; i < size; i += BasicImage<T>::channels) {
        const T value = Traits::average(p + i) >= limit ? white : T{0};
        p[i] = value;
        p[i + 1] = value;
        p[i + 2] = value;
    }
}

template <typename T>
void flip_horizontal(BasicImageView<T> view)
{
    constexpr int channels = BasicImageView<T>::channels;
    for (int y = 0; y < view.height(); ++y) {
        for (int left = 0, right = view.width() - 1; left < right; ++left, --right) {
            std::swap_ranges(view.pixel(left, y), view.pixel(left, y) + channels,
                             view.pixel(right, y));
        }
    }
}

template <typename T>
void flip_horizontal_blocked(BasicImageView<T> view)
{
    constexpr int channels = BasicImageView<T>::channels;
    constexpr int block = 16; // pixels
    for (int y = 0; y < view.height(); ++y) {
        const std::span<T> row = view.row(y);
        int left = 0;
        int right = view.width() - 1;

        // While the two blocks do not overlap: copy both out, write each back
        // to the other end with its pixels in reverse order.
        for (; right - left + 1 >= 2 * block; left += block, right -= block) {
            T* const left_block = row.data() + left * channels;
            T* const right_block = row.data() + (right - block + 1) * channels;
            T from_left[block * channels];
            T from_right[block * channels];
            std::copy_n(left_block, block * channels, from_left);
            std::copy_n(right_block, block * channels, from_right);
            for (int i = 0; i < block; ++i) {
                std::copy_n(from_right + (block - 1 - i) * channels, channels,
                            left_block + i * channels);
                std::copy_n(from_left + (block - 1 - i) * channels, channels,
                            right_block + i * channels);
            }
        }
        // What is left in the middle, fewer than two blocks, pixel by pixel.
        for (; left < right; ++left, --right) {
            std::swap_ranges(row.data() + left * channels,
                             row.data() + (left + 1) * channels, row.data() + right * channels);
        }
    }
}

template <typename T>
void flip_vertical(BasicImageView<T> view)
{
    for (int top = 0, bottom = view.height() - 1; top < bottom; ++top, --bottom) {
        std::ranges::swap_ranges(view.row(top), view.row(bottom));
    }
}

// Column x of the image, read from the bottom up, becomes row x of the result.
template <typename T>
BasicImage<T> rotate_90(const BasicImage<T>& image)
{
    if (image.empty()) {
        return BasicImage<T>();
    }
    constexpr int channels = BasicImage<T>::channels;
    BasicImage<T> rotated(image.height(), image.width());
    for (int x = 0; x < image.width(); ++x) {
        const std::span<T> out = rotated.row(x);
        const std::size_t column = static_cast<std::size_t>(x) * channels;
        for (int y = 0; y < image.height(); ++y) {
            const auto pixel = image.row(image.height() - 1 - y).subspan(column, channels);
            std::ranges::copy(pixel, out.begin() + y * channels);
        }
    }
    return rotated;
}

// The simplest version: every neighbour is visited, and checked against the
// border, for every pixel. Stage v8 moves the border checks out of the inner
// loop and measures the difference.
template <typename T>
void box_blur(BasicImage<T>& image, int radius)
{
    if (radius < 0) {
        throw std::invalid_argument("blur radius must not be negative");
    }
    if (radius == 0 || image.empty()) {
        return;
    }
    using Traits = ChannelTraits<T>;
    constexpr int channels = BasicImage<T>::channels;
    const int width = image.width();
    const int height = image.height();
    // A larger radius covers the whole image and gives the same result, and
    // capping it keeps x + dx below far from int overflow.
    radius = std::min(radius, std::max(width, height));

    // Each output pixel needs the original values of its neighbours, some of
    // which are already overwritten, so the filter reads from a copy.
    const BasicImage<T> source = image;

    for (int y = 0; y < height; ++y) {
        const std::span<T> out = image.row(y);
        for (int x = 0; x < width; ++x) {
            typename Traits::Sum sum[channels] = {};
            int count = 0;
            for (int dy = -radius; dy <= radius; ++dy) {
                for (int dx = -radius; dx <= radius; ++dx) {
                    const int sx = x + dx;
                    const int sy = y + dy;
                    if (sx < 0 || sx >= width || sy < 0 || sy >= height) {
                        continue;
                    }
                    const T* pixel = source.row(sy).data() + sx * channels;
                    for (int c = 0; c < channels; ++c) {
                        sum[c] += pixel[c];
                    }
                    ++count;
                }
            }
            for (int c = 0; c < channels; ++c) {
                out[static_cast<std::size_t>(x) * channels + c] = Traits::mean(sum[c], count);
            }
        }
    }
}

// The neighbour of a pixel at (x, y), where a position outside the image is
// replaced by the nearest position inside it.
template <typename T>
const T* nearest_pixel(const BasicImage<T>& image, int x, int y)
{
    x = std::clamp(x, 0, image.width() - 1);
    y = std::clamp(y, 0, image.height() - 1);
    return image.row(y).data() + x * BasicImage<T>::channels;
}

template <typename T>
void sharpen(BasicImage<T>& image)
{
    if (image.empty()) {
        return;
    }
    using Traits = ChannelTraits<T>;
    using Work = typename Traits::Work;
    constexpr int channels = BasicImage<T>::channels;

    // Neighbours must have their original values, so the filter reads a copy.
    const BasicImage<T> source = image;
    for (int y = 0; y < image.height(); ++y) {
        const std::span<T> out = image.row(y);
        for (int x = 0; x < image.width(); ++x) {
            const T* centre = nearest_pixel(source, x, y);
            const T* left = nearest_pixel(source, x - 1, y);
            const T* right = nearest_pixel(source, x + 1, y);
            const T* above = nearest_pixel(source, x, y - 1);
            const T* below = nearest_pixel(source, x, y + 1);
            for (int c = 0; c < channels; ++c) {
                const Work value = Work{5} * centre[c] - left[c] - right[c] - above[c] - below[c];
                out[static_cast<std::size_t>(x) * channels + c] =
                    static_cast<T>(std::clamp<Work>(value, 0, Traits::max));
            }
        }
    }
}

template <typename T>
void sobel(BasicImage<T>& image)
{
    if (image.empty()) {
        return;
    }
    using Traits = ChannelTraits<T>;
    constexpr int channels = BasicImage<T>::channels;
    const int width = image.width();
    const int height = image.height();

    // The gray level of every pixel, in the units of the channel type.
    std::vector<float> gray(static_cast<std::size_t>(width) * height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            gray[static_cast<std::size_t>(y) * width + x] =
                static_cast<float>(Traits::average(image.row(y).data() + x * channels));
        }
    }
    const auto g = [&](int x, int y) {
        x = std::clamp(x, 0, width - 1);
        y = std::clamp(y, 0, height - 1);
        return gray[static_cast<std::size_t>(y) * width + x];
    };

    const auto limit = static_cast<float>(Traits::max);
    for (int y = 0; y < height; ++y) {
        const std::span<T> out = image.row(y);
        for (int x = 0; x < width; ++x) {
            const float gx = (g(x + 1, y - 1) + 2 * g(x + 1, y) + g(x + 1, y + 1)) -
                             (g(x - 1, y - 1) + 2 * g(x - 1, y) + g(x - 1, y + 1));
            const float gy = (g(x - 1, y + 1) + 2 * g(x, y + 1) + g(x + 1, y + 1)) -
                             (g(x - 1, y - 1) + 2 * g(x, y - 1) + g(x + 1, y - 1));
            const float magnitude = std::min(std::sqrt(gx * gx + gy * gy), limit);
            // 8-bit values are rounded to the nearest whole number.
            const T value = static_cast<T>(Traits::is_8_bit ? magnitude + 0.5f : magnitude);
            for (int c = 0; c < channels; ++c) {
                out[static_cast<std::size_t>(x) * channels + c] = value;
            }
        }
    }
}

template <typename T>
void box_blur_clipped_rows(const BasicImage<T>& source, BasicImage<T>& image, int radius,
                           int y_begin, int y_end)
{
    using Traits = ChannelTraits<T>;
    constexpr int channels = BasicImage<T>::channels;
    const int width = image.width();
    const int height = image.height();
    radius = std::min(radius, std::max(width, height));

    for (int y = y_begin; y < y_end; ++y) {
        const std::span<T> out = image.row(y);
        // The rows of the window that lie inside the image. They are the same
        // for every pixel of this row.
        const int top = std::max(y - radius, 0);
        const int bottom = std::min(y + radius, height - 1);
        for (int x = 0; x < width; ++x) {
            // The columns of the window inside the image, worked out once.
            const int left = std::max(x - radius, 0);
            const int right = std::min(x + radius, width - 1);
            const int count = (right - left + 1) * (bottom - top + 1);

            typename Traits::Sum sum[channels] = {};
            for (int sy = top; sy <= bottom; ++sy) {
                const T* pixel = source.row(sy).data() + left * channels;
                for (int sx = left; sx <= right; ++sx, pixel += channels) {
                    for (int c = 0; c < channels; ++c) {
                        sum[c] += pixel[c];
                    }
                }
            }
            for (int c = 0; c < channels; ++c) {
                out[static_cast<std::size_t>(x) * channels + c] = Traits::mean(sum[c], count);
            }
        }
    }
}

template <typename T>
void box_blur_clipped(BasicImage<T>& image, int radius)
{
    if (radius < 0) {
        throw std::invalid_argument("blur radius must not be negative");
    }
    if (radius == 0 || image.empty()) {
        return;
    }
    const BasicImage<T> source = image;
    box_blur_clipped_rows(source, image, radius, 0, image.height());
}

// Explicit instantiations: the compiler generates each filter for both
// channel types here, so the declarations in filters.hpp are all other files
// need.
template void grayscale(Image&);
template void grayscale(FloatImage&);
template void brightness(Image&, int);
template void brightness(FloatImage&, int);
template void negative(Image&);
template void negative(FloatImage&);
template void threshold(Image&, int);
template void threshold(FloatImage&, int);
template void flip_horizontal(ImageView);
template void flip_horizontal(FloatImageView);
template void flip_horizontal_blocked(ImageView);
template void flip_horizontal_blocked(FloatImageView);
template void flip_vertical(ImageView);
template void flip_vertical(FloatImageView);
template Image rotate_90(const Image&);
template FloatImage rotate_90(const FloatImage&);
template void box_blur(Image&, int);
template void box_blur(FloatImage&, int);
template void box_blur_clipped(Image&, int);
template void box_blur_clipped_rows(const Image&, Image&, int, int, int);
template void box_blur_clipped_rows(const FloatImage&, FloatImage&, int, int, int);
template void box_blur_clipped(FloatImage&, int);
template void sharpen(Image&);
template void sharpen(FloatImage&);
template void sobel(Image&);
template void sobel(FloatImage&);

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

void BoxBlur::apply(Image& image) const
{
    box_blur(image, radius_);
}

std::string BoxBlur::description() const
{
    return std::format("blur {}", radius_);
}

void Sharpen::apply(Image& image) const
{
    sharpen(image);
}

std::string Sharpen::description() const
{
    return "sharpen";
}

void Sobel::apply(Image& image) const
{
    sobel(image);
}

std::string Sobel::description() const
{
    return "sobel";
}

} // namespace pixelforge
