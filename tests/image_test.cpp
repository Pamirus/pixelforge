// Tests for pixelforge::Image: construction, copying and moving.

#include <gtest/gtest.h>

#include "pixelforge/image.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <type_traits>
#include <utility>

using pixelforge::Image;

// std::vector<Image> only moves its elements when it grows if the moves cannot
// throw; otherwise it copies them.
static_assert(std::is_nothrow_move_constructible_v<Image>);
static_assert(std::is_nothrow_move_assignable_v<Image>);

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

TEST(Image, SelfMoveAssignmentKeepsPixels)
{
    Image image = make_test_image(3, 2);
    const Image expected = image;

    Image& same = image;
    image = std::move(same);

    EXPECT_TRUE(same_pixels(image, expected));
}

TEST(Image, SwapExchangesPixels)
{
    Image a = make_test_image(2, 2);
    Image b = make_test_image(5, 1);
    const std::uint8_t* a_pixels = a.data();
    const std::uint8_t* b_pixels = b.data();

    swap(a, b);

    EXPECT_EQ(a.data(), b_pixels);
    EXPECT_EQ(b.data(), a_pixels);
    EXPECT_EQ(a.width(), 5);
    EXPECT_EQ(b.width(), 2);
}
