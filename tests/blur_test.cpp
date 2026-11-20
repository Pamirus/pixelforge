// Tests for pixelforge::Blur<Size>, the box blur with its window size fixed at
// compile time. It must give exactly what box_blur gives with the same window.

#include <gtest/gtest.h>

#include "pixelforge/blur.hpp"
#include "pixelforge/filters.hpp"
#include "pixelforge/image.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <vector>

using pixelforge::Blur;
using pixelforge::FloatImage;
using pixelforge::Image;

// The radius follows from the size, at compile time.
static_assert(Blur<3>::radius == 1);
static_assert(Blur<5>::radius == 2);
static_assert(Blur<21>::radius == 10);

namespace {

Image make_test_image(int width, int height)
{
    Image image(width, height);
    for (std::size_t i = 0; i < image.size(); ++i) {
        image.data()[i] = static_cast<std::uint8_t>(i * 37 % 256);
    }
    return image;
}

template <typename T>
std::vector<T> pixels(const pixelforge::BasicImage<T>& image)
{
    return {image.data(), image.data() + image.size()};
}

// Blurs copies of image with Blur<Size> and with box_blur, and compares them.
// The float sums are added in the same order, so even those match exactly.
template <int Size, typename T>
void expect_same_as_box_blur(const pixelforge::BasicImage<T>& image)
{
    pixelforge::BasicImage<T> fixed = image;
    Blur<Size>{}(fixed);
    pixelforge::BasicImage<T> chosen_at_run_time = image;
    pixelforge::box_blur(chosen_at_run_time, Size / 2);
    EXPECT_EQ(pixels(fixed), pixels(chosen_at_run_time)) << "Blur<" << Size << ">";
}

} // namespace

TEST(Blur, MatchesTheBoxBlurOnEightBitPixels)
{
    const Image image = make_test_image(23, 14);
    expect_same_as_box_blur<3>(image);
    expect_same_as_box_blur<5>(image);
    expect_same_as_box_blur<11>(image);
}

TEST(Blur, MatchesTheBoxBlurOnFloatPixels)
{
    const FloatImage image = pixelforge::to_float(make_test_image(23, 14));
    expect_same_as_box_blur<3>(image);
    expect_same_as_box_blur<5>(image);
    expect_same_as_box_blur<11>(image);
}

// A window wider and taller than the image covers all of it from every pixel.
TEST(Blur, WindowLargerThanTheImageAveragesTheWholeImage)
{
    Image image(3, 1);
    const std::uint8_t channels[] = {0, 0, 0, 60, 60, 60, 90, 90, 90};
    std::copy(std::begin(channels), std::end(channels), image.data());
    Blur<21>{}(image);
    EXPECT_EQ(pixels(image), std::vector<std::uint8_t>(9, 50));
}

TEST(Blur, EmptyImageStaysEmpty)
{
    Image image;
    Blur<3>{}(image);
    EXPECT_TRUE(image.empty());
}
