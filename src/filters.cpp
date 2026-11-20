#include "pixelforge/filters.hpp"

#include "pixelforge/detail/channel_traits.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <span>
#include <stdexcept>
#include <string>

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

template <typename T>
void brightness(BasicImage<T>& image, int amount)
{
    using Traits = ChannelTraits<T>;
    using Work = typename Traits::Work;

    // Larger steps change nothing more, and limiting the amount keeps
    // p[i] + step far away from int overflow.
    const Work step = Traits::from_setting(std::clamp(amount, -255, 255));

    T* p = image.data();
    const std::size_t size = image.size();
    for (std::size_t i = 0; i < size; ++i) {
        p[i] = static_cast<T>(std::clamp<Work>(p[i] + step, 0, Traits::max));
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
template void flip_vertical(ImageView);
template void flip_vertical(FloatImageView);
template Image rotate_90(const Image&);
template FloatImage rotate_90(const FloatImage&);
template void box_blur(Image&, int);
template void box_blur(FloatImage&, int);

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

} // namespace pixelforge
