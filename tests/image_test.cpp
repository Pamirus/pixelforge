// Tests for pixelforge::Image: construction, rows, copying and moving.

#include <gtest/gtest.h>

#include "pixelforge/image.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>

using pixelforge::Image;

// std::vector<Image> only moves its elements when it grows if the moves cannot
// throw; otherwise it copies them.
static_assert(std::is_nothrow_move_constructible_v<Image>);
static_assert(std::is_nothrow_move_assignable_v<Image>);

// A const image gives read-only rows.
static_assert(std::is_same_v<decltype(std::declval<Image&>().row(0)), std::span<std::uint8_t>>);
static_assert(std::is_same_v<decltype(std::declval<const Image&>().row(0)),
                             std::span<const std::uint8_t>>);

namespace {

Image make_test_image(int width, int height)
{
    Image image(width, height);
    for (std::size_t i = 0; i < image.size(); ++i) {
        image.data()[i] = static_cast<std::uint8_t>(i * 37 % 256);
    }
    return image;
}

bool same_pixels(const Image& a, const Image& b)
{
    return a.width() == b.width() && a.height() == b.height() &&
           std::equal(a.data(), a.data() + a.size(), b.data(), b.data() + b.size());
}

} // namespace

TEST(Image, DefaultIsEmpty)
{
    const Image image;
    EXPECT_TRUE(image.empty());
    EXPECT_EQ(image.width(), 0);
    EXPECT_EQ(image.height(), 0);
    EXPECT_EQ(image.size(), 0u);
    EXPECT_EQ(image.data(), nullptr);
}

TEST(Image, NewImageIsBlack)
{
    const Image image(4, 3);
    EXPECT_FALSE(image.empty());
    EXPECT_EQ(image.width(), 4);
    EXPECT_EQ(image.height(), 3);
    EXPECT_EQ(image.size(), 4u * 3u * Image::channels);
    EXPECT_TRUE(std::all_of(image.data(), image.data() + image.size(),
                            [](std::uint8_t v) { return v == 0; }));
}

TEST(Image, RejectsNonPositiveSizes)
{
    EXPECT_THROW(Image(0, 10), std::invalid_argument);
    EXPECT_THROW(Image(10, 0), std::invalid_argument);
    EXPECT_THROW(Image(-1, 10), std::invalid_argument);
}

TEST(Image, RowsFollowEachOtherInMemory)
{
    Image image(4, 3);
    for (int y = 0; y < image.height(); ++y) {
        EXPECT_EQ(image.row(y).size(), std::size_t{4 * Image::channels});
        EXPECT_EQ(image.row(y).data(), image.data() + y * 4 * Image::channels);
    }
}

TEST(Image, WritingToARowChangesOnlyThatRow)
{
    Image image(4, 3);
    std::ranges::fill(image.row(1), std::uint8_t{7});

    EXPECT_TRUE(std::ranges::all_of(image.row(1), [](std::uint8_t v) { return v == 7; }));
    EXPECT_EQ(std::ranges::count(std::span(image.data(), image.size()), 7),
              4 * Image::channels);
}

TEST(Image, CopyIsDeep)
{
    const Image original = make_test_image(5, 2);
    Image copy(original);

    EXPECT_TRUE(same_pixels(copy, original));
    EXPECT_NE(copy.data(), original.data());

    copy.data()[0] = static_cast<std::uint8_t>(original.data()[0] + 1);
    EXPECT_NE(copy.data()[0], original.data()[0]);
}

TEST(Image, CopyOfEmptyImageIsEmpty)
{
    const Image empty;
    const Image copy(empty);
    EXPECT_TRUE(copy.empty());
}

TEST(Image, CopyAssignmentReplacesPixels)
{
    const Image source = make_test_image(2, 2);
    Image target = make_test_image(7, 1);

    target = source;

    EXPECT_TRUE(same_pixels(target, source));
    EXPECT_NE(target.data(), source.data());
}

TEST(Image, SelfCopyAssignmentKeepsPixels)
{
    Image image = make_test_image(3, 3);
    const Image expected = image;
    const std::uint8_t* pixels = image.data();

    Image& same = image; // through a reference, so the compiler does not warn
    image = same;

    EXPECT_EQ(image.data(), pixels);
    EXPECT_TRUE(same_pixels(image, expected));
}

TEST(Image, MoveTakesPixelsAndEmptiesSource)
{
    Image source = make_test_image(4, 4);
    const std::uint8_t* pixels = source.data();

    const Image target(std::move(source));

    EXPECT_EQ(target.data(), pixels); // nothing was copied
    EXPECT_EQ(target.width(), 4);
    EXPECT_EQ(target.height(), 4);
    EXPECT_TRUE(source.empty());
    EXPECT_EQ(source.width(), 0);
    EXPECT_EQ(source.height(), 0);
}

TEST(Image, MoveAssignmentReplacesPixels)
{
    Image source = make_test_image(4, 4);
    const std::uint8_t* pixels = source.data();
    Image target = make_test_image(2, 1);

    target = std::move(source);

    EXPECT_EQ(target.data(), pixels);
    EXPECT_EQ(target.width(), 4);
    EXPECT_TRUE(source.empty());
}

// A standard library object moved into itself is left valid but with
// unspecified contents; libc++ empties the vector. Image inherits that, so the
// test only asks for a consistent image that can be assigned to again.
TEST(Image, SelfMoveAssignmentLeavesValidImage)
{
    Image image = make_test_image(3, 2);

    Image& same = image;
    image = std::move(same);

    EXPECT_EQ(image.size(), static_cast<std::size_t>(image.width()) *
                                static_cast<std::size_t>(image.height()) * Image::channels);
    image = make_test_image(2, 2);
    EXPECT_EQ(image.width(), 2);
    EXPECT_EQ(image.size(), 2u * 2u * Image::channels);
}

TEST(Image, SwapExchangesPixels)
{
    Image a = make_test_image(2, 2);
    Image b = make_test_image(5, 1);
    const std::uint8_t* a_pixels = a.data();
    const std::uint8_t* b_pixels = b.data();

    using std::swap; // a swap found for Image by lookup is used, std::swap otherwise
    swap(a, b);

    EXPECT_EQ(a.data(), b_pixels);
    EXPECT_EQ(b.data(), a_pixels);
    EXPECT_EQ(a.width(), 5);
    EXPECT_EQ(b.width(), 2);
}
