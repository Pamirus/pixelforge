// Is a filter limited by the memory or by the arithmetic? Run the same work on
// images from 48 KB (fits in the fastest cache) up to 50 MB (does not fit in
// any cache), 1024 pixels wide and a growing number of rows:
//
//   copy        std::memcpy between two images: the speed of the memory itself
//   read        add up every byte: what reading alone costs
//   brightness  the filter, in place
//   bright_u8   the same as brightness for a positive amount, but written in 8 bits:
//               add, and take 255 if the sum wrapped around
//   blur r2     box_blur_clipped with radius 2 (25 neighbours per pixel)
//
// Reading the result:
//   - A filter limited by the memory slows down (fewer bytes per second) when
//     the image grows past the cache, and then settles near the speed of
//     `copy`.
//   - A filter limited by the arithmetic takes the same time per pixel for
//     every size, in cache or out of it.
//
//     build/benchmarks/pixelforge_benchmarks --benchmark_filter='bound_'

#include <benchmark/benchmark.h>

#include "pixelforge/filters.hpp"
#include "pixelforge/image.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace {

using pixelforge::Image;

constexpr int width = 1024;

// The argument is the number of rows. 16 rows are 48 KB, 16384 are 50 MB.
void sizes(benchmark::Benchmark* b)
{
    for (const int rows : {16, 128, 1024, 4096, 16384}) {
        b->Arg(rows);
    }
    b->Unit(benchmark::kMicrosecond);
}

void bound_copy(benchmark::State& state)
{
    const Image source(width, static_cast<int>(state.range(0)));
    Image target(width, static_cast<int>(state.range(0)));
    for (auto _ : state) {
        std::memcpy(target.data(), source.data(), source.size());
        benchmark::ClobberMemory();
    }
    // A copy reads and writes every byte, so it moves twice the size.
    state.SetBytesProcessed(state.iterations() * static_cast<std::int64_t>(2 * source.size()));
}

void bound_read(benchmark::State& state)
{
    const Image source(width, static_cast<int>(state.range(0)));
    for (auto _ : state) {
        std::uint64_t sum = 0;
        const std::uint8_t* const p = source.data();
        for (std::size_t i = 0; i < source.size(); ++i) {
            sum += p[i];
        }
        benchmark::DoNotOptimize(sum);
    }
    state.SetBytesProcessed(state.iterations() * static_cast<std::int64_t>(source.size()));
}

void bound_brightness(benchmark::State& state)
{
    Image image(width, static_cast<int>(state.range(0)));
    for (auto _ : state) {
        pixelforge::brightness(image, 20);
        benchmark::ClobberMemory();
    }
    // Read and write every byte, as the copy does.
    state.SetBytesProcessed(state.iterations() * static_cast<std::int64_t>(2 * image.size()));
}

void bound_brightness_u8(benchmark::State& state)
{
    Image image(width, static_cast<int>(state.range(0)));
    const std::uint8_t step = 20;
    for (auto _ : state) {
        std::uint8_t* const p = image.data();
        for (std::size_t i = 0; i < image.size(); ++i) {
            const auto sum = static_cast<std::uint8_t>(p[i] + step);
            p[i] = sum < p[i] ? std::uint8_t{255} : sum;
        }
        benchmark::ClobberMemory();
    }
    state.SetBytesProcessed(state.iterations() * static_cast<std::int64_t>(2 * image.size()));
}

void bound_blur_r2(benchmark::State& state)
{
    Image image(width, static_cast<int>(state.range(0)));
    for (auto _ : state) {
        pixelforge::box_blur_clipped(image, 2);
        benchmark::ClobberMemory();
    }
    // One item = one neighbour pixel read. Most pixels have 25 (the 5 x 5 window).
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(image.size() / 3) * 25);
}

} // namespace

BENCHMARK(bound_copy)->Apply(sizes);
BENCHMARK(bound_read)->Apply(sizes);
BENCHMARK(bound_brightness)->Apply(sizes);
BENCHMARK(bound_brightness_u8)->Apply(sizes);
BENCHMARK(bound_blur_r2)->Apply(sizes);
