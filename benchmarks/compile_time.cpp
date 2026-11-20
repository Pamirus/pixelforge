// The v5 compile-time versions of two filters, on a 3840 x 2160 image, with
// Google Benchmark. They are compared with the run-time versions in
// pixel_types.cpp, so run both together:
//
//     build/benchmarks/pixelforge_benchmarks --benchmark_filter='blur|brightness'
//
// The pixels' values do not change the work these functions do, so a black
// image is as good as a photo here.

#include <benchmark/benchmark.h>

#include "pixelforge/blur.hpp"
#include "pixelforge/filters.hpp"
#include "pixelforge/image.hpp"

#include <cstdint>

namespace {

using pixelforge::BasicImage;
using pixelforge::Image;

constexpr int width = 3840;
constexpr int height = 2160;

// Blur<Size>: the window size is a template parameter. Compare with
// box_blur_image<T>/radius, where Size = 2 * radius + 1.
template <typename T, int Size>
void fixed_blur_image(benchmark::State& state)
{
    BasicImage<T> image(width, height);
    for (auto _ : state) {
        pixelforge::Blur<Size>{}.apply(image);
        benchmark::ClobberMemory();
    }
}

// Brightness through a 256-entry table. Compare with
// brightness_image<std::uint8_t>.
void brightness_lookup_image(benchmark::State& state)
{
    Image image(width, height);
    for (auto _ : state) {
        pixelforge::brightness_lookup(image, 20);
        benchmark::ClobberMemory();
    }
}

} // namespace

BENCHMARK_TEMPLATE(fixed_blur_image, std::uint8_t, 3)->Unit(benchmark::kMillisecond);
BENCHMARK_TEMPLATE(fixed_blur_image, std::uint8_t, 5)->Unit(benchmark::kMillisecond);
BENCHMARK_TEMPLATE(fixed_blur_image, std::uint8_t, 11)->Unit(benchmark::kMillisecond);
BENCHMARK_TEMPLATE(fixed_blur_image, float, 3)->Unit(benchmark::kMillisecond);
BENCHMARK_TEMPLATE(fixed_blur_image, float, 5)->Unit(benchmark::kMillisecond);
BENCHMARK_TEMPLATE(fixed_blur_image, float, 11)->Unit(benchmark::kMillisecond);
BENCHMARK(brightness_lookup_image)->Unit(benchmark::kMillisecond);
