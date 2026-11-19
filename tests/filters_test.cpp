// Tests for the C++ filters in pixelforge/filters.hpp.

#include <gtest/gtest.h>

#include "pf_filters.h"
#include "pf_image.h"
#include "pixelforge/filters.hpp"
#include "pixelforge/image.hpp"

#include <algorithm>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <type_traits>
#include <vector>

using pixelforge::Image;

namespace {

Image make_test_image(int width, int height)
{
    Image image(width, height);
    for (std::size_t i = 0; i < image.size(); ++i) {
        image.data()[i] = static_cast<std::uint8_t>(i * 37 % 256);
    }
    return image;
}

// A width x 1 image whose channel values are given one by one.
Image make_row(std::initializer_list<std::uint8_t> channels)
{
    Image image(static_cast<int>(channels.size() / Image::channels), 1);
    std::copy(channels.begin(), channels.end(), image.data());
    return image;
}

std::vector<std::uint8_t> pixels(const Image& image)
{
    return {image.data(), image.data() + image.size()};
}

// Runs a C filter on a copy of the image's pixels, for comparison.
template <typename CFilter>
std::vector<std::uint8_t> c_result(const Image& image, CFilter filter)
{
    std::vector<std::uint8_t> copy = pixels(image);
    pf_image c_image{image.width(), image.height(), copy.data()};
    filter(&c_image);
    return copy;
}

} // namespace

TEST(Filters, GrayscaleMakesTheThreeChannelsEqual)
{
    Image image = make_test_image(7, 5);
    pixelforge::grayscale(image);
    for (std::size_t i = 0; i < image.size(); i += Image::channels) {
        EXPECT_EQ(image.data()[i], image.data()[i + 1]);
        EXPECT_EQ(image.data()[i], image.data()[i + 2]);
    }
}

TEST(Filters, GrayscaleRoundsTheAverage)
{
    Image image = make_row({0, 0, 1, 0, 1, 1, 10, 20, 30});
    pixelforge::grayscale(image);
    // 1/3 rounds down to 0, 2/3 rounds up to 1, 60/3 is exactly 20.
    EXPECT_EQ(pixels(image), (std::vector<std::uint8_t>{0, 0, 0, 1, 1, 1, 20, 20, 20}));
}

TEST(Filters, BrightnessAddsAndSaturates)
{
    Image image = make_row({0, 100, 250});
    pixelforge::brightness(image, 10);
    EXPECT_EQ(pixels(image), (std::vector<std::uint8_t>{10, 110, 255}));

    pixelforge::brightness(image, -20);
    EXPECT_EQ(pixels(image), (std::vector<std::uint8_t>{0, 90, 235}));
}

TEST(Filters, BrightnessHandlesExtremeAmounts)
{
    Image image = make_row({0, 128, 255});
    pixelforge::brightness(image, INT_MAX);
    EXPECT_EQ(pixels(image), (std::vector<std::uint8_t>{255, 255, 255}));

    pixelforge::brightness(image, INT_MIN);
    EXPECT_EQ(pixels(image), (std::vector<std::uint8_t>{0, 0, 0}));
}

TEST(Filters, NegativeInvertsEveryChannel)
{
    Image image = make_row({0, 1, 255});
    pixelforge::negative(image);
    EXPECT_EQ(pixels(image), (std::vector<std::uint8_t>{255, 254, 0}));
}

TEST(Filters, NegativeTwiceGivesTheOriginal)
{
    const Image original = make_test_image(6, 4);
    Image image = original;
    pixelforge::negative(image);
    pixelforge::negative(image);
    EXPECT_EQ(pixels(image), pixels(original));
}

TEST(Filters, ThresholdSplitsAtTheLevel)
{
    // Gray levels 99, 100 and 101.
    Image image = make_row({99, 99, 99, 100, 100, 100, 100, 101, 102});
    pixelforge::threshold(image, 100);
    EXPECT_EQ(pixels(image), (std::vector<std::uint8_t>{0, 0, 0, 255, 255, 255, 255, 255, 255}));
}

TEST(Filters, ThresholdAtTheEnds)
{
    Image all_white = make_test_image(5, 5);
    pixelforge::threshold(all_white, 0);
    EXPECT_TRUE(std::all_of(all_white.data(), all_white.data() + all_white.size(),
                            [](std::uint8_t v) { return v == 255; }));

    Image all_black = make_test_image(5, 5);
    pixelforge::threshold(all_black, 256);
    EXPECT_TRUE(std::all_of(all_black.data(), all_black.data() + all_black.size(),
                            [](std::uint8_t v) { return v == 0; }));
}

// The README compares the speed of the C and C++ filters, which only makes
// sense if they compute the same thing.
TEST(Filters, GrayscaleMatchesTheCBaseline)
{
    Image image = make_test_image(31, 17);
    const std::vector<std::uint8_t> expected = c_result(image, pf_grayscale);
    pixelforge::grayscale(image);
    EXPECT_EQ(pixels(image), expected);
}

TEST(Filters, BrightnessMatchesTheCBaseline)
{
    for (const int amount : {-300, -20, 0, 20, 300}) {
        Image image = make_test_image(31, 17);
        const std::vector<std::uint8_t> expected =
            c_result(image, [amount](pf_image* c_image) { pf_brightness(c_image, amount); });
        pixelforge::brightness(image, amount);
        EXPECT_EQ(pixels(image), expected) << "amount " << amount;
    }
}

// Filter objects are owned through pointers and never copied (see filters.hpp).
static_assert(std::is_abstract_v<pixelforge::Filter>);
static_assert(std::has_virtual_destructor_v<pixelforge::Filter>);
static_assert(!std::is_copy_constructible_v<pixelforge::Brightness>);

TEST(FilterObjects, GiveTheSameResultAsTheFunctions)
{
    const Image original = make_test_image(16, 9);
    const auto check = [&](const pixelforge::Filter& filter, auto function) {
        Image by_object = original;
        Image by_function = original;
        filter.apply(by_object);
        function(by_function);
        EXPECT_EQ(pixels(by_object), pixels(by_function)) << filter.description();
    };
    check(pixelforge::Grayscale(), [](Image& image) { pixelforge::grayscale(image); });
    check(pixelforge::Brightness(-40), [](Image& image) { pixelforge::brightness(image, -40); });
    check(pixelforge::Negative(), [](Image& image) { pixelforge::negative(image); });
    check(pixelforge::Threshold(90), [](Image& image) { pixelforge::threshold(image, 90); });
}

TEST(FilterObjects, DescribeThemselvesWithTheirSettings)
{
    EXPECT_EQ(pixelforge::Grayscale().description(), "grayscale");
    EXPECT_EQ(pixelforge::Brightness(-20).description(), "brightness -20");
    EXPECT_EQ(pixelforge::Negative().description(), "negative");
    EXPECT_EQ(pixelforge::Threshold(128).description(), "threshold 128");
}

// The command line keeps its filters in a list like this one and runs them in
// order. Brightening a black pixel and then inverting it gives a different
// result than inverting it first.
TEST(FilterObjects, AListRunsInOrder)
{
    std::vector<std::unique_ptr<pixelforge::Filter>> filters;
    filters.push_back(std::make_unique<pixelforge::Brightness>(10));
    filters.push_back(std::make_unique<pixelforge::Negative>());

    const auto run_on_black_pixel = [&] {
        Image image = make_row({0, 0, 0});
        for (const auto& filter : filters) {
            filter->apply(image);
        }
        return pixels(image);
    };
    EXPECT_EQ(run_on_black_pixel(), (std::vector<std::uint8_t>{245, 245, 245}));

    std::ranges::reverse(filters);
    EXPECT_EQ(run_on_black_pixel(), (std::vector<std::uint8_t>{255, 255, 255}));
}
