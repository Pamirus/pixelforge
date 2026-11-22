// The three ways of writing the negative (raw loop, std::transform, ranges)
// must give the same pixels as pixelforge::negative.

#include <gtest/gtest.h>

#include "pixelforge/filters.hpp"
#include "pixelforge/image.hpp"
#include "pixelforge/negative_variants.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

using namespace pixelforge;

namespace {

template <typename T>
BasicImage<T> make_pattern(int width, int height)
{
    BasicImage<T> image(width, height);
    T* p = image.data();
    for (std::size_t i = 0; i < image.size(); ++i) {
        if constexpr (std::is_same_v<T, std::uint8_t>) {
            p[i] = static_cast<T>((i * 37 + 11) % 256);
        } else {
            p[i] = static_cast<T>((i * 37 + 11) % 256) / 255.0f;
        }
    }
    return image;
}

template <typename T, typename Filter>
void expect_same_as_negative(Filter filter)
{
    // Odd sizes, so no vector width divides the pixel count.
    const std::pair<int, int> sizes[] = {{1, 1}, {7, 5}, {64, 3}, {33, 17}};
    for (const auto [w, h] : sizes) {
        const BasicImage<T> original = make_pattern<T>(w, h);
        BasicImage<T> expected = original;
        negative(expected);
        BasicImage<T> actual = original;
        filter(actual);
        for (std::size_t i = 0; i < actual.size(); ++i) {
            ASSERT_EQ(actual.data()[i], expected.data()[i]) << w << "x" << h << " at " << i;
        }
    }
}

} // namespace

TEST(NegativeVariants, RawLoopMatchesNegative)
{
    expect_same_as_negative<std::uint8_t>(variants::negative_raw_loop<std::uint8_t>);
    expect_same_as_negative<float>(variants::negative_raw_loop<float>);
}

TEST(NegativeVariants, TransformMatchesNegative)
{
    expect_same_as_negative<std::uint8_t>(variants::negative_transform<std::uint8_t>);
    expect_same_as_negative<float>(variants::negative_transform<float>);
}

TEST(NegativeVariants, RangesMatchesNegative)
{
    expect_same_as_negative<std::uint8_t>(variants::negative_ranges<std::uint8_t>);
    expect_same_as_negative<float>(variants::negative_ranges<float>);
}

TEST(NegativeVariants, EmptyImageIsLeftAlone)
{
    Image image;
    variants::negative_raw_loop(image);
    variants::negative_transform(image);
    variants::negative_ranges(image);
    EXPECT_TRUE(image.empty());
}
