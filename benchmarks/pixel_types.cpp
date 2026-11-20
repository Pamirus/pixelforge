// The same filters on 8-bit and float pixels, the conversions between them,
// and the box blur, on a 3840 x 2160 image, with Google Benchmark.
//
//     build/benchmarks/pixelforge_benchmarks \
//         --benchmark_filter='grayscale|brightness|convert|blur'
//
// The image takes 24.9 MB with 8-bit pixels and 99.5 MB with float ones. The
// pixels' values do not change the work these functions do, so a black image
// is as good as a photo here.

#include <benchmark/benchmark.h>

#include "pixelforge/filters.hpp"
#include "pixelforge/image.hpp"

#include <cstdint>

namespace {

using pixelforge::BasicImage;
using pixelforge::FloatImage;
using pixelforge::Image;

constexpr int width = 3840;
constexpr int height = 2160;

template <typename T>
void report_bytes(benchmark::State& state, const BasicImage<T>& image)
{
    state.SetBytesProcessed(state.iterations() *
                            static_cast<std::int64_t>(image.size() * sizeof(T)));
}

template <typename T>
void grayscale_image(benchmark::State& state)
{
    BasicImage<T> image(width, height);
    for (auto _ : state) {
        pixelforge::grayscale(image);
        benchmark::ClobberMemory();
    }
    report_bytes(state, image);
}

template <typename T>
void brightness_image(benchmark::State& state)
{
    BasicImage<T> image(width, height);
    for (auto _ : state) {
        pixelforge::brightness(image, 20);
        benchmark::ClobberMemory();
    }
    report_bytes(state, image);
}

// Each iteration allocates the converted image and frees it again.
void convert_to_float(benchmark::State& state)
{
    const Image image(width, height);
    for (auto _ : state) {
        FloatImage converted = pixelforge::to_float(image);
        benchmark::DoNotOptimize(converted.data());
    }
}

void convert_to_uint8(benchmark::State& state)
{
    const FloatImage image(width, height);
    for (auto _ : state) {
        Image converted = pixelforge::to_uint8(image);
        benchmark::DoNotOptimize(converted.data());
    }
}

// The argument is the radius. Each iteration also copies the image, because
// the blur reads the original pixels while it writes the new ones.
template <typename T>
void box_blur_image(benchmark::State& state)
{
    BasicImage<T> image(width, height);
    const int radius = static_cast<int>(state.range(0));
    for (auto _ : state) {
        pixelforge::box_blur(image, radius);
        benchmark::ClobberMemory();
    }
    report_bytes(state, image);
}

} // namespace

BENCHMARK_TEMPLATE(grayscale_image, std::uint8_t)->Unit(benchmark::kMillisecond);
BENCHMARK_TEMPLATE(grayscale_image, float)->Unit(benchmark::kMillisecond);
BENCHMARK_TEMPLATE(brightness_image, std::uint8_t)->Unit(benchmark::kMillisecond);
BENCHMARK_TEMPLATE(brightness_image, float)->Unit(benchmark::kMillisecond);
BENCHMARK(convert_to_float)->Unit(benchmark::kMillisecond);
BENCHMARK(convert_to_uint8)->Unit(benchmark::kMillisecond);
BENCHMARK_TEMPLATE(box_blur_image, std::uint8_t)
    ->Arg(1)->Arg(2)->Arg(5)->Unit(benchmark::kMillisecond);
BENCHMARK_TEMPLATE(box_blur_image, float)
    ->Arg(1)->Arg(2)->Arg(5)->Unit(benchmark::kMillisecond);
