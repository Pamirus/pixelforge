// The histograms counted in parallel must equal the serial one. The racy
// version is not tested here on purpose: it is undefined behaviour, and a
// test that depends on a race going wrong would be flaky. The benchmark
// reports how many counts it loses.

#include <gtest/gtest.h>

#include "pixelforge/histogram.hpp"
#include "pixelforge/image.hpp"

#include <cstddef>
#include <cstdint>
#include <numeric>
#include <utility>

using namespace pixelforge;

namespace {

Image make_pattern(int width, int height)
{
    Image image(width, height);
    std::uint32_t state = 12345;
    for (std::size_t i = 0; i < image.size(); ++i) {
        state = state * 1664525u + 1013904223u;
        image.data()[i] = static_cast<std::uint8_t>(state >> 24);
    }
    return image;
}

const std::pair<int, int> sizes[] = {{1, 1}, {5, 3}, {17, 13}, {200, 100}, {64, 2}};
const unsigned thread_counts[] = {0, 1, 2, 3, 4, 7, 64};

} // namespace

TEST(Histogram, SerialCountsEveryPixelOnce)
{
    const Image image = make_pattern(40, 30);
    const Histogram h = histogram_serial(image);
    EXPECT_EQ(std::accumulate(h.begin(), h.end(), std::uint64_t{0}), 40u * 30u);
}

TEST(Histogram, SerialPutsAPixelInTheBinOfItsAverage)
{
    Image image(2, 1);
    image.data()[0] = 10; image.data()[1] = 20; image.data()[2] = 30; // average 20
    image.data()[3] = 255; image.data()[4] = 255; image.data()[5] = 254; // 254.67 -> 255
    const Histogram h = histogram_serial(image);
    EXPECT_EQ(h[20], 1u);
    EXPECT_EQ(h[255], 1u);
}

TEST(Histogram, AtomicMatchesSerial)
{
    for (const auto [w, h] : sizes) {
        const Image image = make_pattern(w, h);
        for (const unsigned threads : thread_counts) {
            EXPECT_EQ(histogram_atomic(image, threads), histogram_serial(image))
                << w << "x" << h << ", " << threads << " threads";
        }
    }
}

TEST(Histogram, PerThreadMatchesSerial)
{
    for (const auto [w, h] : sizes) {
        const Image image = make_pattern(w, h);
        for (const unsigned threads : thread_counts) {
            EXPECT_EQ(histogram_per_thread(image, threads), histogram_serial(image))
                << w << "x" << h << ", " << threads << " threads";
        }
    }
}

TEST(Histogram, FalseSharingLayoutGivesTheSameCounts)
{
    for (const auto [w, h] : sizes) {
        const Image image = make_pattern(w, h);
        for (const unsigned threads : thread_counts) {
            EXPECT_EQ(histogram_false_sharing(image, threads), histogram_serial(image))
                << w << "x" << h << ", " << threads << " threads";
        }
    }
}

TEST(Histogram, EmptyImageGivesZeros)
{
    const Image image;
    const Histogram zeros{};
    EXPECT_EQ(histogram_serial(image), zeros);
    EXPECT_EQ(histogram_atomic(image, 4), zeros);
    EXPECT_EQ(histogram_per_thread(image, 4), zeros);
    EXPECT_EQ(histogram_false_sharing(image, 4), zeros);
}
