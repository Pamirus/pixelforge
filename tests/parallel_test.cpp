// The filters that split the image across threads must give exactly the
// pixels of the serial filters, for any number of threads.

#include <gtest/gtest.h>

#include "pixelforge/filters.hpp"
#include "pixelforge/image.hpp"
#include "pixelforge/parallel.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

using namespace pixelforge;

namespace {

Image make_pattern(int width, int height)
{
    Image image(width, height);
    for (std::size_t i = 0; i < image.size(); ++i) {
        image.data()[i] = static_cast<std::uint8_t>((i * 53 + 7) % 256);
    }
    return image;
}

void expect_equal(const Image& a, const Image& b)
{
    ASSERT_EQ(a.width(), b.width());
    ASSERT_EQ(a.height(), b.height());
    for (std::size_t i = 0; i < a.size(); ++i) {
        ASSERT_EQ(a.data()[i], b.data()[i]) << "at " << i;
    }
}

const std::pair<int, int> sizes[] = {{1, 1}, {5, 3}, {17, 13}, {64, 2}, {3, 40}};
const unsigned thread_counts[] = {0, 1, 2, 3, 4, 7, 64};

} // namespace

TEST(ParallelChunks, EveryIndexIsVisitedExactlyOnce)
{
    for (const std::size_t count : {0u, 1u, 2u, 7u, 100u, 1001u}) {
        for (const unsigned threads : thread_counts) {
            std::vector<std::atomic<int>> visits(count);
            parallel_chunks(count, threads, [&visits](std::size_t begin, std::size_t end) {
                for (std::size_t i = begin; i < end; ++i) {
                    ++visits[i];
                }
            });
            for (std::size_t i = 0; i < count; ++i) {
                ASSERT_EQ(visits[i].load(), 1) << "count " << count << ", threads " << threads;
            }
        }
    }
}

TEST(Parallel, BrightnessMatchesTheSerialFilter)
{
    for (const auto [w, h] : sizes) {
        for (const unsigned threads : thread_counts) {
            for (const int amount : {60, -90, 0}) {
                Image expected = make_pattern(w, h);
                brightness(expected, amount);
                Image actual = make_pattern(w, h);
                brightness_parallel(actual, amount, threads);
                expect_equal(actual, expected);
            }
        }
    }
}

TEST(Parallel, GrayscaleMatchesTheSerialFilter)
{
    for (const auto [w, h] : sizes) {
        for (const unsigned threads : thread_counts) {
            Image expected = make_pattern(w, h);
            grayscale(expected);
            Image actual = make_pattern(w, h);
            grayscale_parallel(actual, threads);
            expect_equal(actual, expected);
        }
    }
}

TEST(Parallel, BlurMatchesTheSerialFilter)
{
    for (const auto [w, h] : sizes) {
        for (const unsigned threads : thread_counts) {
            for (const int radius : {1, 2, 5, 100}) {
                Image expected = make_pattern(w, h);
                box_blur_clipped(expected, radius);
                Image actual = make_pattern(w, h);
                box_blur_parallel(actual, radius, threads);
                expect_equal(actual, expected);
            }
        }
    }
}

TEST(Parallel, BlurRejectsANegativeRadius)
{
    Image image(4, 4);
    EXPECT_THROW(box_blur_parallel(image, -1, 2), std::invalid_argument);
}

TEST(Parallel, EmptyImagesAreLeftAlone)
{
    Image image;
    brightness_parallel(image, 10, 4);
    grayscale_parallel(image, 4);
    box_blur_parallel(image, 2, 4);
    EXPECT_TRUE(image.empty());
}
