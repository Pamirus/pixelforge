// Tests for pixelforge::ImageView: views and crops that share the image's pixels.

#include <gtest/gtest.h>

#include "pixelforge/image.hpp"
#include "pixelforge/image_view.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <type_traits>

using pixelforge::Image;
using pixelforge::ImageView;

// A view is a pointer and three numbers, so copying one copies no pixels.
static_assert(std::is_trivially_copyable_v<ImageView>);

// Any non-const image converts to a view, but temporaries and const images do
// not (see image_view.hpp). Going back to an Image copies, so it is explicit.
static_assert(std::is_convertible_v<Image&, ImageView>);
static_assert(!std::is_constructible_v<ImageView, Image&&>);
static_assert(!std::is_constructible_v<ImageView, const Image&>);
static_assert(!std::is_convertible_v<ImageView, Image>);

namespace {

// Byte i of the image is i. The images in these tests are under 256 bytes.
Image numbered_image(int width, int height)
{
    Image image(width, height);
    for (std::size_t i = 0; i < image.size(); ++i) {
        image.data()[i] = static_cast<std::uint8_t>(i);
    }
    return image;
}

} // namespace

TEST(ImageView, DefaultIsEmpty)
{
    const ImageView view;
    EXPECT_TRUE(view.empty());
    EXPECT_EQ(view.width(), 0);
    EXPECT_EQ(view.height(), 0);
}

TEST(ImageView, ViewOfAnEmptyImageIsEmpty)
{
    Image image;
    EXPECT_TRUE(ImageView(image).empty());
}

TEST(ImageView, WholeImageViewUsesTheImagePixels)
{
    Image image(4, 3);
    const ImageView view(image);

    EXPECT_EQ(view.width(), 4);
    EXPECT_EQ(view.height(), 3);
    EXPECT_EQ(view.stride(), 4 * 3);
    EXPECT_EQ(view.pixel(0, 0), image.data());
    EXPECT_EQ(view.pixel(3, 2), image.data() + image.size() - 3);
}

TEST(ImageView, CropPointsIntoTheImage)
{
    Image image(4, 3);
    const ImageView crop = ImageView(image).crop(1, 1, 2, 2);

    EXPECT_EQ(crop.width(), 2);
    EXPECT_EQ(crop.height(), 2);
    EXPECT_EQ(crop.stride(), 4 * 3); // still the row length of the whole image
    EXPECT_EQ(crop.pixel(0, 0), image.data() + (1 * 4 + 1) * 3);
    EXPECT_EQ(crop.pixel(1, 1), image.data() + (2 * 4 + 2) * 3);
}

TEST(ImageView, WritingThroughACropChangesOnlyThatRegion)
{
    Image image(4, 3);
    const ImageView crop = ImageView(image).crop(1, 1, 2, 2);
    for (int y = 0; y < crop.height(); ++y) {
        for (int x = 0; x < crop.width(); ++x) {
            std::fill_n(crop.pixel(x, y), Image::channels, std::uint8_t{255});
        }
    }

    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const bool inside = x >= 1 && x <= 2 && y >= 1 && y <= 2;
            const std::uint8_t expected = inside ? 255 : 0;
            const std::uint8_t* p = image.data() + (y * image.width() + x) * Image::channels;
            EXPECT_TRUE(std::all_of(p, p + Image::channels,
                                    [=](std::uint8_t v) { return v == expected; }))
                << "pixel " << x << ", " << y;
        }
    }
}

TEST(ImageView, CropOfACropCountsFromItsOwnCorner)
{
    Image image(5, 4);
    const ImageView whole(image);
    const ImageView outer = whole.crop(1, 1, 3, 3);
    const ImageView inner = outer.crop(1, 2, 2, 1);

    EXPECT_EQ(inner.pixel(0, 0), whole.pixel(2, 3));
    EXPECT_EQ(inner.pixel(1, 0), whole.pixel(3, 3));

    // It must fit inside the outer crop, not just inside the image.
    EXPECT_THROW((void)outer.crop(0, 0, 4, 1), std::out_of_range);
}

TEST(ImageView, CropMustFitInsideTheView)
{
    Image image(4, 3);
    const ImageView view(image);

    EXPECT_NO_THROW((void)view.crop(0, 0, 4, 3));
    EXPECT_NO_THROW((void)view.crop(3, 2, 1, 1));
    EXPECT_THROW((void)view.crop(-1, 0, 2, 2), std::out_of_range);
    EXPECT_THROW((void)view.crop(0, -1, 2, 2), std::out_of_range);
    EXPECT_THROW((void)view.crop(3, 0, 2, 1), std::out_of_range); // one column too many
    EXPECT_THROW((void)view.crop(0, 2, 1, 2), std::out_of_range); // one row too many
    EXPECT_THROW((void)view.crop(1, 0, std::numeric_limits<int>::max(), 1),
                 std::out_of_range); // x + width would overflow an int
    EXPECT_THROW((void)view.crop(0, 0, 0, 1), std::invalid_argument);
    EXPECT_THROW((void)view.crop(0, 0, 1, -1), std::invalid_argument);
}

TEST(ImageView, ImageFromAViewCopiesItsPixels)
{
    Image image = numbered_image(4, 3);
    const Image copy(ImageView(image).crop(1, 1, 2, 2));

    ASSERT_EQ(copy.width(), 2);
    ASSERT_EQ(copy.height(), 2);
    const std::uint8_t expected[] = {
        15, 16, 17, 18, 19, 20, // row 1, pixels 1 and 2
        27, 28, 29, 30, 31, 32, // row 2, pixels 1 and 2
    };
    EXPECT_TRUE(std::equal(copy.data(), copy.data() + copy.size(),
                           std::begin(expected), std::end(expected)));

    // A copy, not another view: changing the image leaves it alone.
    std::fill_n(image.data(), image.size(), std::uint8_t{0});
    EXPECT_EQ(copy.data()[0], 15);
}

TEST(ImageView, ImageFromAnEmptyViewIsEmpty)
{
    const Image copy{ImageView()};
    EXPECT_TRUE(copy.empty());
    EXPECT_EQ(copy.width(), 0);
}
