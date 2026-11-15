#include "pixelforge/io.hpp"

#include <stb_image.h>
#include <stb_image_write.h>

#include <algorithm>
#include <cctype>
#include <memory>
#include <string>
#include <system_error>

namespace pixelforge {

namespace {

// stb returns the decoded pixels in a malloc'd buffer. Holding it in a
// unique_ptr frees it on every way out of load, including a throw.
struct StbFree {
    void operator()(stbi_uc* pixels) const noexcept { stbi_image_free(pixels); }
};
using StbPixels = std::unique_ptr<stbi_uc, StbFree>;

[[noreturn]] void fail(const char* action, const std::filesystem::path& path,
                       const std::string& reason)
{
    throw IoError(std::string(action) + " " + path.string() + ": " + reason);
}

std::string lowercase_extension(const std::filesystem::path& path)
{
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return extension;
}

} // namespace

Image load(const std::filesystem::path& path)
{
    // stb would only say "can't fopen", so a missing file is checked first.
    std::error_code error;
    if (!std::filesystem::exists(path, error)) {
        fail("cannot load", path, "no such file");
    }

    int width = 0;
    int height = 0;
    int channels_in_file = 0;
    const StbPixels pixels{
        stbi_load(path.string().c_str(), &width, &height, &channels_in_file, Image::channels)};
    if (!pixels) {
        const char* reason = stbi_failure_reason();
        fail("cannot load", path, reason != nullptr ? reason : "unknown error");
    }

    Image image(width, height);
    std::copy_n(pixels.get(), image.size(), image.data());
    return image;
}

void save(const Image& image, const std::filesystem::path& path)
{
    if (image.empty()) {
        fail("cannot save", path, "the image is empty");
    }

    const std::string file = path.string();
    const std::string extension = lowercase_extension(path);
    const int width = image.width();
    const int height = image.height();
    int written = 0;

    if (extension == ".png") {
        written = stbi_write_png(file.c_str(), width, height, Image::channels, image.data(),
                                 width * Image::channels);
    } else if (extension == ".jpg" || extension == ".jpeg") {
        written = stbi_write_jpg(file.c_str(), width, height, Image::channels, image.data(), 90);
    } else if (extension == ".bmp") {
        written = stbi_write_bmp(file.c_str(), width, height, Image::channels, image.data());
    } else {
        fail("cannot save", path, "unsupported format (use .png, .jpg or .bmp)");
    }

    if (written == 0) {
        fail("cannot save", path, "could not write the file");
    }
}

} // namespace pixelforge
