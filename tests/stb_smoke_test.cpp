// Checks that the vendored stb libraries are compiled and linked correctly:
// a small image encoded to PNG in memory must decode back to the same pixels.

#include <gtest/gtest.h>
#include <stb_image.h>
#include <stb_image_write.h>

#include <algorithm>
#include <array>
#include <memory>
#include <vector>

namespace {

// stb_image_write hands the encoded bytes to this callback chunk by chunk.
void append_to_buffer(void* context, void* data, int size)
{
    auto* buffer = static_cast<std::vector<unsigned char>*>(context);
    const auto* bytes = static_cast<const unsigned char*>(data);
    buffer->insert(buffer->end(), bytes, bytes + size);
}

} // namespace

TEST(Stb, PngRoundTripKeepsPixels)
{
    constexpr int width = 2;
    constexpr int height = 2;
    constexpr int channels = 3;
    // clang-format off
    const std::array<unsigned char, width * height * channels> pixels{
        255,   0,   0,     0, 255,   0,  // red,  green
          0,   0, 255,   255, 255, 255,  // blue, white
    };
    // clang-format on

    std::vector<unsigned char> png;
    ASSERT_NE(stbi_write_png_to_func(append_to_buffer, &png, width, height, channels,
                                     pixels.data(), width * channels),
              0);

    int decoded_width = 0;
    int decoded_height = 0;
    int channels_in_file = 0;
    const std::unique_ptr<unsigned char, decltype(&stbi_image_free)> decoded{
        stbi_load_from_memory(png.data(), static_cast<int>(png.size()), &decoded_width,
                              &decoded_height, &channels_in_file, channels),
        &stbi_image_free};
    ASSERT_NE(decoded.get(), nullptr) << stbi_failure_reason();

    EXPECT_EQ(decoded_width, width);
    EXPECT_EQ(decoded_height, height);
    EXPECT_EQ(channels_in_file, channels);
    EXPECT_TRUE(std::equal(pixels.begin(), pixels.end(), decoded.get()));
}
