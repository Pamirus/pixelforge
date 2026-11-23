#include "pixelforge/histogram.hpp"

#include "pixelforge/detail/channel_traits.hpp"
#include "pixelforge/parallel.hpp"

#include <atomic>
#include <cstddef>
#include <vector>

namespace pixelforge {

namespace {

using detail::ChannelTraits;

std::uint8_t gray_level(const std::uint8_t* pixel)
{
    return ChannelTraits<std::uint8_t>::average(pixel);
}

std::size_t pixel_count(const Image& image)
{
    return image.size() / Image::channels;
}

// A thread's own counters, aligned to a cache line so that the counters of one
// thread never share a line with those of the next.
struct alignas(64) PrivateBins {
    std::array<std::uint64_t, 256> bins{};
};

} // namespace

Histogram histogram_serial(const Image& image)
{
    Histogram result{};
    const std::uint8_t* const p = image.data();
    for (std::size_t n = 0, count = pixel_count(image); n < count; ++n) {
        ++result[gray_level(p + n * Image::channels)];
    }
    return result;
}

// UNDEFINED BEHAVIOUR ON PURPOSE. See histogram.hpp.
Histogram histogram_racy(const Image& image, unsigned threads)
{
    Histogram shared{};
    std::uint64_t* const bins = shared.data();
    const std::uint8_t* const p = image.data();
    parallel_chunks(pixel_count(image), threads, [=](std::size_t begin, std::size_t end) {
        for (std::size_t n = begin; n < end; ++n) {
            ++bins[gray_level(p + n * Image::channels)]; // read, add, write: not atomic
        }
    });
    return shared;
}

Histogram histogram_atomic(const Image& image, unsigned threads)
{
    std::array<std::atomic<std::uint64_t>, 256> shared{};
    const std::uint8_t* const p = image.data();
    auto* const bins = shared.data();
    parallel_chunks(pixel_count(image), threads, [=](std::size_t begin, std::size_t end) {
        for (std::size_t n = begin; n < end; ++n) {
            // relaxed: only the final totals matter, not the order of the adds.
            bins[gray_level(p + n * Image::channels)].fetch_add(1, std::memory_order_relaxed);
        }
    });
    Histogram result;
    for (std::size_t v = 0; v < result.size(); ++v) {
        result[v] = shared[v].load();
    }
    return result;
}

Histogram histogram_per_thread(const Image& image, unsigned threads)
{
    // One set of counters per thread; piece t counts into set t.
    const unsigned n_threads = threads == 0 ? 1 : threads;
    std::vector<PrivateBins> sets(n_threads);
    const std::uint8_t* const p = image.data();
    PrivateBins* const all = sets.data();
    parallel_chunks_indexed(pixel_count(image), threads,
                            [=](unsigned t, std::size_t begin, std::size_t end) {
        PrivateBins& mine = all[t];
        for (std::size_t n = begin; n < end; ++n) {
            ++mine.bins[gray_level(p + n * Image::channels)];
        }
    });
    Histogram result{};
    for (const PrivateBins& set : sets) {
        for (std::size_t v = 0; v < result.size(); ++v) {
            result[v] += set.bins[v];
        }
    }
    return result;
}

Histogram histogram_false_sharing(const Image& image, unsigned threads)
{
    // counters[v * n_threads + t]: the counter of gray level v of thread t. All
    // the counters of one level, from every thread, are next to each other.
    const unsigned n_threads = threads == 0 ? 1 : threads;
    std::vector<std::uint64_t> counters(256u * n_threads);
    const std::uint8_t* const p = image.data();
    std::uint64_t* const c = counters.data();
    parallel_chunks_indexed(pixel_count(image), threads,
                            [=](unsigned t, std::size_t begin, std::size_t end) {
        for (std::size_t n = begin; n < end; ++n) {
            ++c[gray_level(p + n * Image::channels) * n_threads + t];
        }
    });
    Histogram result{};
    for (std::size_t v = 0; v < result.size(); ++v) {
        for (unsigned t = 0; t < n_threads; ++t) {
            result[v] += counters[v * n_threads + t];
        }
    }
    return result;
}

} // namespace pixelforge
