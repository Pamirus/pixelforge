#pragma once

// An 8-bit image stored in planes: all the red values, then all the green
// values, then all the blue ones (RRR…GGG…BBB…), where Image keeps each
// pixel's three values together (RGBRGB…). The two layouts hold the same
// information. Which one is faster depends on how an operation reads it, and
// stage v8 measures that.
//
//   interleaved:  R0 G0 B0 R1 G1 B1 R2 G2 B2 …    pixel-wise work reads one
//                                                  stretch of memory
//   planar:       R0 R1 R2 … G0 G1 G2 … B0 B1 B2 … work on one channel reads
//                                                  only that channel's plane

#include "pixelforge/image.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace pixelforge {

class PlanarImage {
public:
    static constexpr int channels = Image::channels;

    // An empty image: no pixels, width and height 0.
    PlanarImage() = default;

    // A black width × height image. Throws std::invalid_argument if width or
    // height is not positive, and std::length_error if the image is too large,
    // like Image does.
    PlanarImage(int width, int height);

    int width() const noexcept { return data_.empty() ? 0 : width_; }
    int height() const noexcept { return data_.empty() ? 0 : height_; }
    bool empty() const noexcept { return data_.empty(); }

    // Number of values in all three planes together.
    std::size_t size() const noexcept { return data_.size(); }

    // The width × height values of one channel: 0 is red, 1 green, 2 blue.
    // Not checked, like operator[].
    std::span<std::uint8_t> plane(int channel) noexcept
    {
        return {data_.data() + plane_size() * static_cast<std::size_t>(channel), plane_size()};
    }
    std::span<const std::uint8_t> plane(int channel) const noexcept
    {
        return {data_.data() + plane_size() * static_cast<std::size_t>(channel), plane_size()};
    }

    // All three planes in one block, red first.
    std::uint8_t* data() noexcept { return data_.data(); }
    const std::uint8_t* data() const noexcept { return data_.data(); }

private:
    std::size_t plane_size() const noexcept { return data_.size() / channels; }

    std::vector<std::uint8_t> data_;
    int width_ = 0;
    int height_ = 0;
};

// The same pixels in the other layout. Both copy every value.
[[nodiscard]] PlanarImage to_planar(const Image& image);
[[nodiscard]] Image to_interleaved(const PlanarImage& image);

// The filters of filters.hpp that a planar layout changes, with the same
// arithmetic, so the two layouts give the same pixels.
void grayscale(PlanarImage& image);
void brightness(PlanarImage& image, int amount);

// Adds amount to one channel only, clamped to 0..255: 0 is red, 1 green, 2
// blue. Throws std::invalid_argument for another channel number. There is a
// version for each layout. A planar image reads and writes only that
// channel's plane; an interleaved one has to step through all of its memory
// to reach every third value.
void brighten_channel(PlanarImage& image, int channel, int amount);
void brighten_channel(Image& image, int channel, int amount);

} // namespace pixelforge
