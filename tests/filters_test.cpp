// Tests for the C++ filters in pixelforge/filters.hpp.

#include <gtest/gtest.h>

#include "pf_filters.h"
#include "pf_image.h"
#include "pixelforge/filters.hpp"
#include "pixelforge/image.hpp"
#include "pixelforge/image_view.hpp"

#include <algorithm>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <type_traits>
#include <utility>
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

// Byte i of the image is i, so every channel of every pixel differs. The
// images built with it are under 256 bytes.
Image numbered_image(int width, int height)
{
    Image image(width, height);
    for (std::size_t i = 0; i < image.size(); ++i) {
        image.data()[i] = static_cast<std::uint8_t>(i);
    }
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

// Pixels move as whole pixels: the channels inside each one keep their order.
TEST(Filters, FlipHorizontalMirrorsEachRow)
{
    Image image = numbered_image(3, 2);
    pixelforge::flip_horizontal(image);
    const std::vector<std::uint8_t> expected = {
        6,  7,  8,  3,  4,  5,  0, 1,  2,  // row 0: pixels 2, 1, 0
        15, 16, 17, 12, 13, 14, 9, 10, 11, // row 1: pixels 5, 4, 3
    };
    EXPECT_EQ(pixels(image), expected);
}

TEST(Filters, FlipVerticalSwapsTheRows)
{
    Image image = numbered_image(2, 3);
    pixelforge::flip_vertical(image);
    const std::vector<std::uint8_t> expected = {
        12, 13, 14, 15, 16, 17, // was row 2
        6,  7,  8,  9,  10, 11, // row 1 stays
        0,  1,  2,  3,  4,  5,  // was row 0
    };
    EXPECT_EQ(pixels(image), expected);
}

TEST(Filters, FlippingTwiceGivesTheOriginal)
{
    for (const auto [width, height] : {std::pair{7, 5}, std::pair{6, 4}, std::pair{1, 1}}) {
        const Image original = make_test_image(width, height);
        Image image = original;
        pixelforge::flip_horizontal(image);
        pixelforge::flip_horizontal(image);
        pixelforge::flip_vertical(image);
        pixelforge::flip_vertical(image);
        EXPECT_EQ(pixels(image), pixels(original)) << width << " x " << height;
    }
}

// The flips take a view, so they can mirror just part of an image.
TEST(Filters, FlipsChangeOnlyTheCrop)
{
    Image image = numbered_image(4, 3);
    pixelforge::flip_horizontal(pixelforge::ImageView(image).crop(1, 0, 2, 3));
    // Pixels 1 and 2 of each row swap; pixels 0 and 3 stay where they are.
    const std::vector<std::uint8_t> first_row = {0, 1, 2, 6, 7, 8, 3, 4, 5, 9, 10, 11};
    EXPECT_TRUE(std::ranges::equal(image.row(0), first_row));

    Image other = numbered_image(4, 3);
    pixelforge::flip_vertical(pixelforge::ImageView(other).crop(0, 1, 4, 2));
    const Image original = numbered_image(4, 3);
    EXPECT_TRUE(std::ranges::equal(other.row(0), original.row(0)));
    EXPECT_TRUE(std::ranges::equal(other.row(1), original.row(2)));
    EXPECT_TRUE(std::ranges::equal(other.row(2), original.row(1)));
}

// A 3 x 2 image becomes 2 x 3. Reading pixels as letters:
//
//   A B C        D A
//   D E F   ->   E B
//                F C
TEST(Filters, Rotate90TurnsClockwise)
{
    const Image image = numbered_image(3, 2);
    const Image rotated = pixelforge::rotate_90(image);

    EXPECT_EQ(rotated.width(), 2);
    EXPECT_EQ(rotated.height(), 3);
    const std::vector<std::uint8_t> expected = {
        9,  10, 11, 0, 1, 2, // D A
        12, 13, 14, 3, 4, 5, // E B
        15, 16, 17, 6, 7, 8, // F C
    };
    EXPECT_EQ(pixels(rotated), expected);
    EXPECT_EQ(pixels(image), pixels(numbered_image(3, 2))); // the original is unchanged
}

TEST(Filters, FourRotationsGiveTheOriginal)
{
    const Image original = make_test_image(7, 5);
    Image image = original;
    for (int turn = 0; turn < 4; ++turn) {
        image = pixelforge::rotate_90(image);
        EXPECT_EQ(image.width(), turn % 2 == 0 ? 5 : 7);
    }
    EXPECT_EQ(image.height(), 5);
    EXPECT_EQ(pixels(image), pixels(original));
}

TEST(Filters, RotatingAnEmptyImageGivesAnEmptyImage)
{
    EXPECT_TRUE(pixelforge::rotate_90(Image()).empty());
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
        EXPECT_EQ(by_object.width(), by_function.width()) << filter.description();
        EXPECT_EQ(pixels(by_object), pixels(by_function)) << filter.description();
    };
    check(pixelforge::Grayscale(), [](Image& image) { pixelforge::grayscale(image); });
    check(pixelforge::Brightness(-40), [](Image& image) { pixelforge::brightness(image, -40); });
    check(pixelforge::Negative(), [](Image& image) { pixelforge::negative(image); });
    check(pixelforge::Threshold(90), [](Image& image) { pixelforge::threshold(image, 90); });
    check(pixelforge::FlipHorizontal(), [](Image& image) { pixelforge::flip_horizontal(image); });
    check(pixelforge::FlipVertical(), [](Image& image) { pixelforge::flip_vertical(image); });
    check(pixelforge::Rotate90(), [](Image& image) { image = pixelforge::rotate_90(image); });
}

TEST(FilterObjects, DescribeThemselvesWithTheirSettings)
{
    EXPECT_EQ(pixelforge::Grayscale().description(), "grayscale");
    EXPECT_EQ(pixelforge::Brightness(-20).description(), "brightness -20");
    EXPECT_EQ(pixelforge::Negative().description(), "negative");
    EXPECT_EQ(pixelforge::Threshold(128).description(), "threshold 128");
    EXPECT_EQ(pixelforge::FlipHorizontal().description(), "flip horizontal");
    EXPECT_EQ(pixelforge::FlipVertical().description(), "flip vertical");
    EXPECT_EQ(pixelforge::Rotate90().description(), "rotate 90");
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
