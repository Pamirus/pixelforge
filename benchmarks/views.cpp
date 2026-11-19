// Views, flips and rotation on a 3840 x 2160 image (24.9 MB), with Google
// Benchmark.
//
//     build/benchmarks/pixelforge_benchmarks --benchmark_filter='crop|flip|rotate'
//
// The pixels' values do not change the work these functions do, so a black
// image is as good as a photo here.

#include <benchmark/benchmark.h>

#include "pixelforge/filters.hpp"
#include "pixelforge/image.hpp"
#include "pixelforge/image_view.hpp"

#include <cstdint>

namespace {

using pixelforge::Image;
using pixelforge::ImageView;

constexpr int width = 3840;
constexpr int height = 2160;

// The middle quarter of the image: 1920 x 1080 pixels, 6.2 MB.
ImageView middle_quarter(Image& image)
{
    return ImageView(image).crop(width / 4, height / 4, width / 2, height / 2);
}

void report_bytes(benchmark::State& state, std::int64_t bytes_per_iteration)
{
    state.SetBytesProcessed(state.iterations() * bytes_per_iteration);
}

// A crop only computes a pointer and checks four numbers.
void crop_view(benchmark::State& state)
{
    Image image(width, height);
    for (auto _ : state) {
        ImageView view = middle_quarter(image);
        benchmark::DoNotOptimize(view);
    }
}

// The same crop copied into an Image of its own, row by row.
void crop_copy_into_new_image(benchmark::State& state)
{
    Image image(width, height);
    const ImageView view = middle_quarter(image);
    for (auto _ : state) {
        Image copy(view);
        benchmark::DoNotOptimize(copy.data());
    }
    report_bytes(state, std::int64_t{view.width()} * view.height() * Image::channels);
}

void flip_horizontal_image(benchmark::State& state)
{
    Image image(width, height);
    for (auto _ : state) {
        pixelforge::flip_horizontal(image);
        benchmark::ClobberMemory();
    }
    report_bytes(state, static_cast<std::int64_t>(image.size()));
}

void flip_vertical_image(benchmark::State& state)
{
    Image image(width, height);
    for (auto _ : state) {
        pixelforge::flip_vertical(image);
        benchmark::ClobberMemory();
    }
    report_bytes(state, static_cast<std::int64_t>(image.size()));
}

// Each iteration allocates the rotated image and frees it again.
void rotate_90_image(benchmark::State& state)
{
    const Image image(width, height);
    for (auto _ : state) {
        Image rotated = pixelforge::rotate_90(image);
        benchmark::DoNotOptimize(rotated.data());
    }
    report_bytes(state, static_cast<std::int64_t>(image.size()));
}

} // namespace

BENCHMARK(crop_view)->Unit(benchmark::kNanosecond);
BENCHMARK(crop_copy_into_new_image)->Unit(benchmark::kMillisecond);
BENCHMARK(flip_horizontal_image)->Unit(benchmark::kMillisecond);
BENCHMARK(flip_vertical_image)->Unit(benchmark::kMillisecond);
BENCHMARK(rotate_90_image)->Unit(benchmark::kMillisecond);
