// Tests for pixelforge::PlanarImage: the same pixels as an Image, stored one
// channel after the other.

#include <gtest/gtest.h>

#include "pixelforge/filters.hpp"
#include "pixelforge/image.hpp"
#include "pixelforge/planar.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <vector>

using pixelforge::Image;
using pixelforge::PlanarImage;

namespace {

Image make_test_image(int width, int height)
{
    Image image(width, height);
    for (std::size_t i = 0; i < image.size(); ++i) {
        image.data()[i] = static_cast<std::uint8_t>(i * 37 % 256);
    }
    return image;
}

std::vector<std::uint8_t> pixels(const Image& image)
{
    return {image.data(), image.data() + image.size()};
}

} // namespace

TEST(PlanarImage, DefaultIsEmpty)
{
    const PlanarImage image;
    EXPECT_TRUE(image.empty());
    EXPECT_EQ(image.width(), 0);
    EXPECT_EQ(image.height(), 0);
    EXPECT_EQ(image.size(), 0u);
}

TEST(PlanarImage, ConstructorMakesABlackImageAndChecksTheSize)
{
    const PlanarImage image(4, 3);
    EXPECT_EQ(image.width(), 4);
    EXPECT_EQ(image.height(), 3);
    EXPECT_EQ(image.size(), 4u * 3 * 3);
    EXPECT_EQ(image.plane(0).size(), 12u);
    for (std::size_t i = 0; i < image.size(); ++i) {
        ASSERT_EQ(image.data()[i], 0);
    }
    EXPECT_THROW(PlanarImage(0, 3), std::invalid_argument);
    EXPECT_THROW(PlanarImage(3, -1), std::invalid_argument);
    EXPECT_THROW(PlanarImage(std::numeric_limits<int>::max(), std::numeric_limits<int>::max()),
                 std::length_error);
}

// The planes follow each other in one block: red, then green, then blue.
TEST(PlanarImage, PlanesAreConsecutiveInOneBlock)
{
    PlanarImage image(2, 2);
    EXPECT_EQ(image.plane(0).data(), image.data());
    EXPECT_EQ(image.plane(1).data(), image.data() + 4);
    EXPECT_EQ(image.plane(2).data(), image.data() + 8);
}

TEST(PlanarImage, ConversionPutsEachChannelInItsPlane)
{
    Image image(2, 1);
    const std::uint8_t values[] = {1, 2, 3, 4, 5, 6}; // pixel 0 is (1,2,3), pixel 1 is (4,5,6)
    std::copy(std::begin(values), std::end(values), image.data());

    const PlanarImage planar = pixelforge::to_planar(image);
    EXPECT_EQ(planar.plane(0)[0], 1);
    EXPECT_EQ(planar.plane(0)[1], 4);
    EXPECT_EQ(planar.plane(1)[0], 2);
    EXPECT_EQ(planar.plane(1)[1], 5);
    EXPECT_EQ(planar.plane(2)[0], 3);
    EXPECT_EQ(planar.plane(2)[1], 6);
}

TEST(PlanarImage, ConversionRoundTripsAndKeepsEmptyImagesEmpty)
{
    const Image original = make_test_image(13, 7);
    const Image back = pixelforge::to_interleaved(pixelforge::to_planar(original));
    EXPECT_EQ(back.width(), 13);
    EXPECT_EQ(back.height(), 7);
    EXPECT_EQ(pixels(back), pixels(original));

    EXPECT_TRUE(pixelforge::to_planar(Image()).empty());
    EXPECT_TRUE(pixelforge::to_interleaved(PlanarImage()).empty());
}

// Each planar operation must give the pixels of the interleaved one.
TEST(PlanarImage, GrayscaleMatchesTheInterleavedFilter)
{
    const Image original = make_test_image(13, 7);
    Image expected = original;
    pixelforge::grayscale(expected);

    PlanarImage planar = pixelforge::to_planar(original);
    pixelforge::grayscale(planar);
    EXPECT_EQ(pixels(pixelforge::to_interleaved(planar)), pixels(expected));
}

TEST(PlanarImage, BrightnessMatchesTheInterleavedFilter)
{
    const Image original = make_test_image(13, 7);
    for (const int amount : {-1000, -60, 0, 33, 255, 1000}) {
        Image expected = original;
        pixelforge::brightness(expected, amount);

        PlanarImage planar = pixelforge::to_planar(original);
        pixelforge::brightness(planar, amount);
        EXPECT_EQ(pixels(pixelforge::to_interleaved(planar)), pixels(expected))
            << "amount " << amount;
    }
}

TEST(PlanarImage, BrightenChannelChangesOnlyThatChannel)
{
    const Image original = make_test_image(13, 7);
    for (int channel = 0; channel < 3; ++channel) {
        Image interleaved = original;
        pixelforge::brighten_channel(interleaved, channel, 40);

        PlanarImage planar = pixelforge::to_planar(original);
        pixelforge::brighten_channel(planar, channel, 40);

        EXPECT_EQ(pixels(pixelforge::to_interleaved(planar)), pixels(interleaved))
            << "channel " << channel;

        // The other two channels are as they were.
        for (std::size_t i = 0; i < original.size(); ++i) {
            if (static_cast<int>(i % 3) != channel) {
                ASSERT_EQ(interleaved.data()[i], original.data()[i]) << i;
            }
        }
    }
}

TEST(PlanarImage, BrightenChannelClampsAndRejectsABadChannel)
{
    Image image(1, 1);
    image.data()[0] = 250;
    pixelforge::brighten_channel(image, 0, 100);
    EXPECT_EQ(image.data()[0], 255);
    pixelforge::brighten_channel(image, 0, -1000);
    EXPECT_EQ(image.data()[0], 0);

    PlanarImage planar(1, 1);
    EXPECT_THROW(pixelforge::brighten_channel(planar, 3, 1), std::invalid_argument);
    EXPECT_THROW(pixelforge::brighten_channel(planar, -1, 1), std::invalid_argument);
    EXPECT_THROW(pixelforge::brighten_channel(image, 3, 1), std::invalid_argument);
}

TEST(PlanarImage, OperationsOnAnEmptyImageDoNothing)
{
    PlanarImage planar;
    pixelforge::grayscale(planar);
    pixelforge::brightness(planar, 10);
    pixelforge::brighten_channel(planar, 1, 10);
    EXPECT_TRUE(planar.empty());
}
