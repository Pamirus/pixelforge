#pragma once

#include "pixelforge/image.hpp"

#include <filesystem>
#include <stdexcept>

namespace pixelforge {

// Thrown by load and save. what() names the file and the reason, for example
// "cannot load cat.png: no such file".
class IoError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Reads any format stb_image understands (PNG, JPEG, BMP, ...) and converts it
// to RGB. Throws IoError if the file is missing or cannot be decoded.
[[nodiscard]] Image load(const std::filesystem::path& path);

// Writes the image in the format named by the extension, in any letter case:
// .png, .jpg or .jpeg (quality 90), or .bmp. Throws IoError for an empty
// image, any other extension, or a file that cannot be written.
void save(const Image& image, const std::filesystem::path& path);

} // namespace pixelforge
