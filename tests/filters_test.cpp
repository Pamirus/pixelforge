// Tests for the C++ filters in pixelforge/filters.hpp, on 8-bit and float
// images.

#include <gtest/gtest.h>

#include "pf_filters.h"
#include "pf_image.h"
#include "pixelforge/filters.hpp"
#include "pixelforge/image.hpp"
#include "pixelforge/image_view.hpp"

#include <algorithm>
#include <climits>
#include <cstdlib>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <stdexcept>
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

// brightness_table is constexpr, so these checks run while the test is
// compiled: a wrong table would stop the build.
static_assert(pixelforge::brightness_table(0)[0] == 0);
static_assert(pixelforge::brightness_table(0)[255] == 255);
static_assert(pixelforge::brightness_table(10)[100] == 110);
static_assert(pixelforge::brightness_table(10)[250] == 255);
static_assert(pixelforge::brightness_table(-20)[10] == 0);
static_assert(pixelforge::brightness_table(INT_MAX)[0] == 255);
static_assert(pixelforge::brightness_table(INT_MIN)[255] == 0);

// Every amount from beyond -255 to beyond 255, on an image that holds every
// channel value from 0 to 255.
TEST(Filters, BrightnessLookupMatchesBrightness)
{
    Image all_values(256, 1);
    for (std::size_t i = 0; i < all_values.size(); ++i) {
        all_values.data()[i] = static_cast<std::uint8_t>(i / Image::channels);
    }
    for (int amount = -300; amount <= 300; ++amount) {
        Image computed = all_values;
        pixelforge::brightness(computed, amount);
        Image looked_up = all_values;
        pixelforge::brightness_lookup(looked_up, amount);
        ASSERT_EQ(pixels(looked_up), pixels(computed)) << "amount " << amount;
    }
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

// Every width around the block size (16 pixels) and the point where two
// blocks no longer fit (32), odd and even, so that the block loop, the
// leftover loop and a width with no middle pixel are all used.
TEST(Filters, BlockedHorizontalFlipGivesTheSameImageAsTheOtherFlip)
{
    for (const int width : {1, 2, 3, 15, 16, 17, 31, 32, 33, 47, 48, 49, 100}) {
        const Image original = make_test_image(width, 3);
        Image expected = original;
        pixelforge::flip_horizontal(expected);
        Image blocked = original;
        pixelforge::flip_horizontal_blocked(blocked);
        EXPECT_EQ(pixels(blocked), pixels(expected)) << "width " << width;
    }
}

TEST(Filters, BlockedHorizontalFlipWorksOnACropAndOnFloats)
{
    // A crop 40 pixels wide inside a wider image: only its pixels move.
    const Image original = make_test_image(64, 4);
    Image expected = original;
    pixelforge::flip_horizontal(pixelforge::ImageView(expected).crop(10, 1, 40, 2));
    Image blocked = original;
    pixelforge::flip_horizontal_blocked(pixelforge::ImageView(blocked).crop(10, 1, 40, 2));
    EXPECT_EQ(pixels(blocked), pixels(expected));

    pixelforge::FloatImage floats = pixelforge::to_float(make_test_image(37, 2));
    pixelforge::FloatImage floats_expected = floats;
    pixelforge::flip_horizontal(floats_expected);
    pixelforge::flip_horizontal_blocked(floats);
    EXPECT_TRUE(std::equal(floats.data(), floats.data() + floats.size(), floats_expected.data()));
}

TEST(Filters, RotatingAnEmptyImageGivesAnEmptyImage)
{
    EXPECT_TRUE(pixelforge::rotate_90(Image()).empty());
}

// A 3 x 3 image, black except for the middle pixel (90, 30, 0). With radius 1,
// each pixel averages the pixels of its 3 x 3 window that lie inside the
// image: 9 in the middle, 6 at an edge, 4 in a corner.
TEST(Filters, BoxBlurAveragesTheNeighboursInsideTheImage)
{
    Image image(3, 3);
    std::uint8_t* middle = image.data() + 4 * Image::channels;
    middle[0] = 90;
    middle[1] = 30;
    pixelforge::box_blur(image, 1);

    const auto pixel = [&](int x, int y) {
        const std::uint8_t* p = image.data() + (y * 3 + x) * Image::channels;
        return std::vector<std::uint8_t>(p, p + Image::channels);
    };
    EXPECT_EQ(pixel(1, 1), (std::vector<std::uint8_t>{10, 3, 0})); // 90 / 9, 30 / 9
    EXPECT_EQ(pixel(1, 0), (std::vector<std::uint8_t>{15, 5, 0})); // 90 / 6, 30 / 6
    EXPECT_EQ(pixel(0, 0), (std::vector<std::uint8_t>{23, 8, 0})); // 90 / 4 = 22.5, 30 / 4 = 7.5
}

// Leaving out the neighbours outside the image, instead of counting them as
// black, keeps the border as bright as the rest.
TEST(Filters, BoxBlurKeepsAUniformImageUnchanged)
{
    Image image(5, 4);
    std::fill_n(image.data(), image.size(), std::uint8_t{77});
    pixelforge::box_blur(image, 2);
    EXPECT_TRUE(std::all_of(image.data(), image.data() + image.size(),
                            [](std::uint8_t v) { return v == 77; }));
}

TEST(Filters, BoxBlurWithAHugeRadiusAveragesTheWholeImage)
{
    Image image = make_row({0, 0, 0, 60, 60, 60, 90, 90, 90});
    pixelforge::box_blur(image, 1'000'000);
    EXPECT_EQ(pixels(image), (std::vector<std::uint8_t>(9, 50)));
}

// The clipped blur must give the pixels of box_blur for every image shape and
// radius: a single pixel, a single row or column, windows that cover the whole
// image, and ones that touch a border on one side only.
TEST(Filters, ClippedBoxBlurGivesTheSameImageAsBoxBlur)
{
    const int sizes[][2] = {{1, 1}, {1, 7}, {7, 1}, {2, 2}, {7, 5}, {20, 3}, {13, 17}};
    for (const auto& size : sizes) {
        const Image original = make_test_image(size[0], size[1]);
        for (const int radius : {0, 1, 2, 3, 5, 100}) {
            Image expected = original;
            pixelforge::box_blur(expected, radius);
            Image clipped = original;
            pixelforge::box_blur_clipped(clipped, radius);
            EXPECT_EQ(pixels(clipped), pixels(expected))
                << size[0] << "x" << size[1] << " radius " << radius;
        }
    }
}

// The sums are added in the same order, so even float results are equal.
TEST(FloatFilters, ClippedBoxBlurGivesExactlyTheSameImageAsBoxBlur)
{
    const pixelforge::FloatImage original = pixelforge::to_float(make_test_image(19, 11));
    for (const int radius : {1, 2, 4, 30}) {
        pixelforge::FloatImage expected = original;
        pixelforge::box_blur(expected, radius);
        pixelforge::FloatImage clipped = original;
        pixelforge::box_blur_clipped(clipped, radius);
        EXPECT_TRUE(std::equal(clipped.data(), clipped.data() + clipped.size(), expected.data()))
            << "radius " << radius;
    }
}

TEST(Filters, ClippedBoxBlurRejectsANegativeRadiusAndAcceptsAnEmptyImage)
{
    Image image = make_test_image(3, 3);
    EXPECT_THROW(pixelforge::box_blur_clipped(image, -1), std::invalid_argument);
    Image empty;
    pixelforge::box_blur_clipped(empty, 2);
    EXPECT_TRUE(empty.empty());
}

TEST(Filters, BoxBlurRadiusZeroChangesNothingAndNegativeThrows)
{
    const Image original = make_test_image(6, 5);
    Image image = original;
    pixelforge::box_blur(image, 0);
    EXPECT_EQ(pixels(image), pixels(original));
    EXPECT_THROW(pixelforge::box_blur(image, -1), std::invalid_argument);
}

// A width x height image whose R, G and B are all the given gray values, row by row.
Image gray_image(int width, int height, std::initializer_list<std::uint8_t> grays)
{
    Image image(width, height);
    auto gray = grays.begin();
    for (std::size_t i = 0; i < image.size(); i += Image::channels, ++gray) {
        std::fill_n(image.data() + i, Image::channels, *gray);
    }
    return image;
}

// Five times the pixel minus its four neighbours. A neighbour outside the
// image is the nearest pixel inside it, and the result is clamped.
TEST(Filters, SharpenKeepsFlatAreasAndBrightensAnIsolatedPixel)
{
    Image flat(4, 3);
    std::fill_n(flat.data(), flat.size(), std::uint8_t{77});
    pixelforge::sharpen(flat);
    EXPECT_TRUE(std::all_of(flat.data(), flat.data() + flat.size(),
                            [](std::uint8_t v) { return v == 77; }));

    // A white pixel in the middle of a black 3 x 3 image: 5 * 200 - 0 is
    // clamped to 255, and each neighbour is 5 * 0 - 200, clamped to 0.
    Image dot(3, 3);
    std::fill_n(dot.data() + 4 * Image::channels, Image::channels, std::uint8_t{200});
    pixelforge::sharpen(dot);
    EXPECT_EQ(pixels(dot).at(4 * 3), 255);
    for (const int neighbour : {1, 3, 5, 7}) {
        EXPECT_EQ(pixels(dot).at(neighbour * 3), 0) << "pixel " << neighbour;
    }
    for (const int corner : {0, 2, 6, 8}) {
        EXPECT_EQ(pixels(dot).at(corner * 3), 0) << "pixel " << corner;
    }
}

TEST(Filters, SharpenUsesTheNearestPixelOutsideTheBorder)
{
    // Pixels 10, 20, 30 in one row. Above and below are the pixel itself.
    //   10: 5 * 10 - 10 (left) - 20 - 10 - 10 = 0
    //   20: 5 * 20 - 10 - 30 - 20 - 20        = 20
    //   30: 5 * 30 - 20 - 30 (right) - 30 - 30 = 40
    Image image = gray_image(3, 1, {10, 20, 30});
    pixelforge::sharpen(image);
    EXPECT_EQ(pixels(image), (std::vector<std::uint8_t>{0, 0, 0, 20, 20, 20, 40, 40, 40}));
}

// A vertical edge: the left two columns are 0, the right two are 10.
// At columns 1 and 2, gx = 4 * 10 - 0 = 40 and gy = 0. Elsewhere both are 0.
TEST(Filters, SobelFindsAVerticalEdge)
{
    Image image = gray_image(4, 3, {0, 0, 10, 10, 0, 0, 10, 10, 0, 0, 10, 10});
    pixelforge::sobel(image);
    for (int y = 0; y < 3; ++y) {
        for (int x = 0; x < 4; ++x) {
            const std::uint8_t expected = (x == 1 || x == 2) ? 40 : 0;
            const std::uint8_t* p = image.data() + (y * 4 + x) * Image::channels;
            EXPECT_EQ(p[0], expected) << x << "," << y;
            EXPECT_EQ(p[1], expected);
            EXPECT_EQ(p[2], expected);
        }
    }
}

TEST(Filters, SobelFindsAHorizontalEdge)
{
    Image image = gray_image(2, 4, {0, 0, 0, 0, 10, 10, 10, 10});
    pixelforge::sobel(image);
    for (int y = 0; y < 4; ++y) {
        const std::uint8_t expected = (y == 1 || y == 2) ? 40 : 0;
        EXPECT_EQ(image.data()[y * 2 * Image::channels], expected) << "row " << y;
    }
}

TEST(Filters, SobelClampsStrongEdgesAndIgnoresFlatAreas)
{
    Image edge = gray_image(4, 1, {0, 0, 255, 255});
    pixelforge::sobel(edge);
    EXPECT_EQ(edge.data()[1 * Image::channels], 255); // 4 * 255 = 1020, clamped

    Image flat(5, 5);
    std::fill_n(flat.data(), flat.size(), std::uint8_t{200});
    pixelforge::sobel(flat);
    EXPECT_TRUE(std::all_of(flat.data(), flat.data() + flat.size(),
                            [](std::uint8_t v) { return v == 0; }));
}

TEST(Filters, SharpenAndSobelLeaveAnEmptyImageEmpty)
{
    Image image;
    pixelforge::sharpen(image);
    pixelforge::sobel(image);
    EXPECT_TRUE(image.empty());
}

// The filters are templates, so each call below runs the 8-bit or the float
// version depending on the image. Converting the float result back to 8 bits
// must give exactly the 8-bit result. (Threshold is left out: a gray level
// exactly at the limit can land on either side of it in float.)
TEST(FloatFilters, AgreeWithTheEightBitFilters)
{
    const Image original = make_test_image(31, 17);
    const auto check = [&](const char* name, auto filter) {
        Image bytes = original;
        filter(bytes);
        pixelforge::FloatImage floats = pixelforge::to_float(original);
        filter(floats);
        const Image from_floats = pixelforge::to_uint8(floats);
        EXPECT_EQ(from_floats.width(), bytes.width()) << name;
        EXPECT_EQ(pixels(from_floats), pixels(bytes)) << name;
    };
    check("grayscale", [](auto& image) { pixelforge::grayscale(image); });
    check("brightness 40", [](auto& image) { pixelforge::brightness(image, 40); });
    check("brightness -90", [](auto& image) { pixelforge::brightness(image, -90); });
    check("negative", [](auto& image) { pixelforge::negative(image); });
    check("flip horizontal", [](auto& image) { pixelforge::flip_horizontal(image); });
    check("flip vertical", [](auto& image) { pixelforge::flip_vertical(image); });
    check("rotate 90", [](auto& image) { image = pixelforge::rotate_90(image); });
}

// sharpen is exact integer arithmetic on 8 bits and the same sum in float, so
// the results agree to the rounding of the conversion. In sobel the 8-bit gray
// level is rounded to a whole number and the float one is not; that error
// reaches the gradients with weights up to 2 + 1 + 1 on each side, so the
// results may differ by a few levels.
TEST(FloatFilters, SharpenAndSobelAgreeWithTheEightBitFilters)
{
    const Image original = make_test_image(31, 17);
    const auto largest_difference = [&](auto filter) {
        Image bytes = original;
        filter(bytes);
        pixelforge::FloatImage floats = pixelforge::to_float(original);
        filter(floats);
        const Image from_floats = pixelforge::to_uint8(floats);
        int largest = 0;
        for (std::size_t i = 0; i < bytes.size(); ++i) {
            const int difference = int{bytes.data()[i]} - int{from_floats.data()[i]};
            largest = std::max(largest, std::abs(difference));
        }
        return largest;
    };
    EXPECT_LE(largest_difference([](auto& image) { pixelforge::sharpen(image); }), 1);
    EXPECT_LE(largest_difference([](auto& image) { pixelforge::sobel(image); }), 6);
}

// A blurred 8-bit channel is a rounded mean, and in a corner window of 4 or an
// edge window of 6 pixels that mean can end in exactly .5. The float result
// can then round to either side, so here the two may differ by 1.
TEST(FloatFilters, BoxBlurAgreesWithTheEightBitBlurWithinOne)
{
    const Image original = make_test_image(31, 17);
    Image bytes = original;
    pixelforge::box_blur(bytes, 2);
    pixelforge::FloatImage floats = pixelforge::to_float(original);
    pixelforge::box_blur(floats, 2);
    const Image from_floats = pixelforge::to_uint8(floats);

    int largest_difference = 0;
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        largest_difference = std::max(largest_difference,
                                      std::abs(int{bytes.data()[i]} - int{from_floats.data()[i]}));
    }
    EXPECT_LE(largest_difference, 1);
}

TEST(FloatFilters, GrayscaleIsNotRounded)
{
    pixelforge::FloatImage image(1, 1);
    image.data()[0] = 0.1f;
    image.data()[1] = 0.2f;
    image.data()[2] = 0.4f;
    pixelforge::grayscale(image);
    for (int c = 0; c < Image::channels; ++c) {
        EXPECT_FLOAT_EQ(image.data()[c], 0.7f / 3);
    }
}

// The amount is on the 0 to 255 scale: 51 adds 51 / 255 = 0.2.
TEST(FloatFilters, BrightnessClampsToZeroAndOne)
{
    pixelforge::FloatImage image(1, 1);
    image.data()[0] = 0.9f;
    image.data()[1] = 0.1f;
    image.data()[2] = 0.5f;

    pixelforge::brightness(image, 51);
    EXPECT_FLOAT_EQ(image.data()[0], 1.0f);
    EXPECT_FLOAT_EQ(image.data()[1], 0.3f);
    EXPECT_FLOAT_EQ(image.data()[2], 0.7f);

    pixelforge::brightness(image, -255);
    EXPECT_EQ(image.data()[0], 0.0f);
    EXPECT_EQ(image.data()[1], 0.0f);
    EXPECT_EQ(image.data()[2], 0.0f);
}

// The level is on the 0 to 255 scale too: 128 means 128 / 255, about 0.502.
TEST(FloatFilters, ThresholdUsesTheLevelOver255)
{
    pixelforge::FloatImage image(2, 1);
    std::fill_n(image.data(), 3, 0.6f);
    std::fill_n(image.data() + 3, 3, 0.4f);
    pixelforge::threshold(image, 128);
    const std::vector<float> expected = {1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f};
    EXPECT_TRUE(std::equal(image.data(), image.data() + image.size(), expected.begin()));
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
    check(pixelforge::BoxBlur(2), [](Image& image) { pixelforge::box_blur(image, 2); });
    check(pixelforge::Sharpen(), [](Image& image) { pixelforge::sharpen(image); });
    check(pixelforge::Sobel(), [](Image& image) { pixelforge::sobel(image); });
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
    EXPECT_EQ(pixelforge::BoxBlur(3).description(), "blur 3");
    EXPECT_EQ(pixelforge::Sharpen().description(), "sharpen");
    EXPECT_EQ(pixelforge::Sobel().description(), "sobel");
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
