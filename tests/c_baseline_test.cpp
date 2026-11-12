// Tests for the C baseline in baseline_c/.

#include <gtest/gtest.h>

#include "pf_image.h"

#include <algorithm>
#include <cstddef>
#include <filesystem>
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
