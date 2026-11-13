// Tests for the C baseline in baseline_c/.

#include <gtest/gtest.h>

#include "pf_filters.h"
#include "pf_image.h"

#include <algorithm>
#include <climits>
#include <cstddef>
#include <filesystem>
#include <initializer_list>
#include <memory>
#include <string>

namespace {

// The C code frees images by hand; in the tests a unique_ptr does it, so a
// failing ASSERT does not leak.
struct ImageDeleter {
    void operator()(pf_image* image) const { pf_image_destroy(image); }
};
using ImagePtr = std::unique_ptr<pf_image, ImageDeleter>;

ImagePtr make_test_image(int width, int height)
{
    ImagePtr image{pf_image_create(width, height)};
    if (image) {
        for (std::size_t i = 0; i < pf_image_size(image.get()); ++i) {
            image->pixels[i] = static_cast<unsigned char>(i * 37 % 256);
        }
    }
    return image;
}

// A width x 1 image whose channel values are given one by one.
ImagePtr make_row(std::initializer_list<unsigned char> channels)
{
    ImagePtr image{pf_image_create(static_cast<int>(channels.size() / PF_CHANNELS), 1)};
    if (image) {
        std::copy(channels.begin(), channels.end(), image->pixels);
    }
    return image;
}

std::filesystem::path temp_file(const std::string& name)
{
    return std::filesystem::temp_directory_path() / ("pixelforge_test_" + name);
}

} // namespace

TEST(CImage, CreateAllocatesThreeBytesPerPixel)
{
    const ImagePtr image{pf_image_create(4, 3)};
    ASSERT_NE(image, nullptr) << pf_image_last_error();
    EXPECT_EQ(image->width, 4);
    EXPECT_EQ(image->height, 3);
    EXPECT_EQ(pf_image_size(image.get()), 4u * 3u * 3u);
}

TEST(CImage, CreateRejectsSizesThatAreNotPositive)
{
    EXPECT_EQ(pf_image_create(0, 10), nullptr);
    EXPECT_EQ(pf_image_create(10, -1), nullptr);
    EXPECT_STRNE(pf_image_last_error(), "");
}

TEST(CImage, PngRoundTripKeepsPixels)
{
    const ImagePtr original = make_test_image(5, 3);
    ASSERT_NE(original, nullptr);

    const std::string path = temp_file("roundtrip.png").string();
    ASSERT_EQ(pf_image_save(original.get(), path.c_str()), 0) << pf_image_last_error();
    const ImagePtr loaded{pf_image_load(path.c_str())};
    std::filesystem::remove(path);
    ASSERT_NE(loaded, nullptr) << pf_image_last_error();

    EXPECT_EQ(loaded->width, 5);
    EXPECT_EQ(loaded->height, 3);
    EXPECT_TRUE(std::equal(original->pixels, original->pixels + pf_image_size(original.get()),
                           loaded->pixels));
}

TEST(CImage, JpegRoundTripKeepsSize)
{
    const ImagePtr original = make_test_image(16, 8);
    ASSERT_NE(original, nullptr);

    const std::string path = temp_file("roundtrip.JPG").string();
    ASSERT_EQ(pf_image_save(original.get(), path.c_str()), 0) << pf_image_last_error();
    const ImagePtr loaded{pf_image_load(path.c_str())};
    std::filesystem::remove(path);
    ASSERT_NE(loaded, nullptr) << pf_image_last_error();

    // JPEG is lossy, so only the size is compared.
    EXPECT_EQ(loaded->width, 16);
    EXPECT_EQ(loaded->height, 8);
}

TEST(CImage, LoadOfMissingFileFails)
{
    const ImagePtr image{pf_image_load("no/such/file.png")};
    EXPECT_EQ(image, nullptr);
    EXPECT_STRNE(pf_image_last_error(), "");
}

TEST(CImage, SaveRejectsUnknownExtension)
{
    const ImagePtr image = make_test_image(2, 2);
    ASSERT_NE(image, nullptr);
    EXPECT_EQ(pf_image_save(image.get(), "out.gif"), -1);
    EXPECT_STRNE(pf_image_last_error(), "");
}

TEST(CFilters, GrayscaleMakesTheThreeChannelsEqual)
{
    const ImagePtr image = make_test_image(7, 5);
    ASSERT_NE(image, nullptr);
    pf_grayscale(image.get());
    for (std::size_t i = 0; i < pf_image_size(image.get()); i += PF_CHANNELS) {
        ASSERT_EQ(image->pixels[i], image->pixels[i + 1]) << "pixel " << i / PF_CHANNELS;
        ASSERT_EQ(image->pixels[i + 1], image->pixels[i + 2]) << "pixel " << i / PF_CHANNELS;
    }
}

TEST(CFilters, GrayscaleRoundsTheAverage)
{
    // Averages: 61/3 = 20.33, 62/3 = 20.67, 765/3 = 255, 0/3 = 0.
    const ImagePtr image = make_row({10, 20, 31, 10, 20, 32, 255, 255, 255, 0, 0, 0});
    ASSERT_NE(image, nullptr);
    pf_grayscale(image.get());
    EXPECT_EQ(image->pixels[0], 20);
    EXPECT_EQ(image->pixels[3], 21);
    EXPECT_EQ(image->pixels[6], 255);
    EXPECT_EQ(image->pixels[9], 0);
}

TEST(CFilters, BrightnessAddsToEveryChannel)
{
    const ImagePtr image = make_row({0, 100, 200});
    ASSERT_NE(image, nullptr);
    pf_brightness(image.get(), 30);
    EXPECT_EQ(image->pixels[0], 30);
    EXPECT_EQ(image->pixels[1], 130);
    EXPECT_EQ(image->pixels[2], 230);
}

TEST(CFilters, BrightnessSaturatesAt255And0)
{
    const ImagePtr image = make_row({0, 100, 250});
    ASSERT_NE(image, nullptr);
    pf_brightness(image.get(), 10);
    EXPECT_EQ(image->pixels[2], 255);
    pf_brightness(image.get(), -50);
    EXPECT_EQ(image->pixels[0], 0);
    EXPECT_EQ(image->pixels[1], 60);
    EXPECT_EQ(image->pixels[2], 205);
}

TEST(CFilters, BrightnessHandlesExtremeAmountsWithoutOverflow)
{
    const ImagePtr image = make_row({0, 128, 255});
    ASSERT_NE(image, nullptr);
    const auto all_equal = [&image](unsigned char value) {
        return std::all_of(image->pixels, image->pixels + 3,
                           [value](unsigned char c) { return c == value; });
    };
    pf_brightness(image.get(), INT_MAX);
    EXPECT_TRUE(all_equal(255));
    pf_brightness(image.get(), INT_MIN);
    EXPECT_TRUE(all_equal(0));
}
