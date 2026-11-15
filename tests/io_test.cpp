// Tests for pixelforge::load and pixelforge::save.

#include <gtest/gtest.h>

#include "pixelforge/image.hpp"
#include "pixelforge/io.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

using pixelforge::Image;
using pixelforge::IoError;

namespace {

Image make_test_image(int width, int height)
{
    Image image(width, height);
    for (std::size_t i = 0; i < image.size(); ++i) {
        image.data()[i] = static_cast<std::uint8_t>(i * 37 % 256);
    }
    return image;
}

std::filesystem::path temp_file(const std::string& name)
{
    return std::filesystem::temp_directory_path() / ("pixelforge_io_test_" + name);
}

// Runs action and returns the message of the IoError it throws, or "" if it
// does not throw one.
template <typename Action>
std::string io_error_message(Action action)
{
    try {
        action();
    } catch (const IoError& error) {
        return error.what();
    }
    return "";
}

} // namespace

TEST(Io, PngRoundTripKeepsPixels)
{
    const Image original = make_test_image(5, 3);
    const auto path = temp_file("roundtrip.png");

    pixelforge::save(original, path);
    const Image loaded = pixelforge::load(path);
    std::filesystem::remove(path);

    EXPECT_EQ(loaded.width(), 5);
    EXPECT_EQ(loaded.height(), 3);
    EXPECT_TRUE(std::equal(original.data(), original.data() + original.size(), loaded.data(),
                           loaded.data() + loaded.size()));
}

TEST(Io, BmpRoundTripKeepsPixels)
{
    const Image original = make_test_image(4, 4);
    const auto path = temp_file("roundtrip.bmp");

    pixelforge::save(original, path);
    const Image loaded = pixelforge::load(path);
    std::filesystem::remove(path);

    EXPECT_TRUE(std::equal(original.data(), original.data() + original.size(), loaded.data(),
                           loaded.data() + loaded.size()));
}

TEST(Io, JpegRoundTripKeepsSize)
{
    const Image original = make_test_image(16, 8);
    const auto path = temp_file("roundtrip.JPG"); // the extension check ignores case

    pixelforge::save(original, path);
    const Image loaded = pixelforge::load(path);
    std::filesystem::remove(path);

    // JPEG is lossy, so only the size is compared.
    EXPECT_EQ(loaded.width(), 16);
    EXPECT_EQ(loaded.height(), 8);
}

TEST(Io, LoadOfMissingFileThrows)
{
    EXPECT_EQ(io_error_message([] { (void)pixelforge::load("no/such/file.png"); }),
              "cannot load no/such/file.png: no such file");
}

TEST(Io, LoadOfFileThatIsNotAnImageThrows)
{
    const auto path = temp_file("not-an-image.png");
    std::ofstream(path) << "this is text, not a PNG\n";

    const std::string message = io_error_message([&] { (void)pixelforge::load(path); });
    std::filesystem::remove(path);

    EXPECT_EQ(message.rfind("cannot load " + path.string() + ": ", 0), 0u) << message;
}

TEST(Io, SaveRejectsUnknownExtension)
{
    const auto path = temp_file("out.gif");

    EXPECT_EQ(io_error_message([&] { pixelforge::save(make_test_image(2, 2), path); }),
              "cannot save " + path.string() + ": unsupported format (use .png, .jpg or .bmp)");
    EXPECT_FALSE(std::filesystem::exists(path));
}

TEST(Io, SaveRejectsEmptyImage)
{
    EXPECT_EQ(io_error_message([] { pixelforge::save(Image{}, "empty.png"); }),
              "cannot save empty.png: the image is empty");
}

TEST(Io, SaveIntoMissingDirectoryThrows)
{
    const Image image = make_test_image(2, 2);
    EXPECT_EQ(io_error_message([&] { pixelforge::save(image, "no/such/dir/out.png"); }),
              "cannot save no/such/dir/out.png: could not write the file");
}
