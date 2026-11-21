// Interleaved (Image: RGBRGB...) against planar (PlanarImage: RRR...GGG...BBB...)
// storage on a 3840 x 2160 image (24.9 MB), with Google Benchmark.
//
//     build/benchmarks/pixelforge_benchmarks --benchmark_filter='interleaved|planar'
//
// The pixels' values do not change the work these functions do, so a black
// image is as good as a photo here.

#include <benchmark/benchmark.h>

#include "pixelforge/filters.hpp"
#include "pixelforge/image.hpp"
#include "pixelforge/planar.hpp"

#include <cstdint>

namespace {

using pixelforge::Image;
using pixelforge::PlanarImage;

constexpr int width = 3840;
constexpr int height = 2160;
constexpr int red = 0;

void report_bytes(benchmark::State& state, std::size_t bytes_per_iteration)
{
    state.SetBytesProcessed(state.iterations() * static_cast<std::int64_t>(bytes_per_iteration));
}

void grayscale_interleaved(benchmark::State& state)
{
    Image image(width, height);
    for (auto _ : state) {
        pixelforge::grayscale(image);
        benchmark::ClobberMemory();
    }
    report_bytes(state, image.size());
}

void grayscale_planar(benchmark::State& state)
{
    PlanarImage image(width, height);
    for (auto _ : state) {
        pixelforge::grayscale(image);
        benchmark::ClobberMemory();
    }
    report_bytes(state, image.size());
}

// The same amount added to every value, so the layout should not matter.
void brightness_interleaved(benchmark::State& state)
{
    Image image(width, height);
    for (auto _ : state) {
        pixelforge::brightness(image, 20);
        benchmark::ClobberMemory();
    }
    report_bytes(state, image.size());
}

void brightness_planar(benchmark::State& state)
{
    PlanarImage image(width, height);
    for (auto _ : state) {
        pixelforge::brightness(image, 20);
        benchmark::ClobberMemory();
    }
    report_bytes(state, image.size());
}

// One channel only. The bytes counted are the ones the operation changes: a
// third of the image.
void brighten_red_interleaved(benchmark::State& state)
{
    Image image(width, height);
    for (auto _ : state) {
        pixelforge::brighten_channel(image, red, 20);
        benchmark::ClobberMemory();
    }
    report_bytes(state, image.size() / 3);
}

void brighten_red_planar(benchmark::State& state)
{
    PlanarImage image(width, height);
    for (auto _ : state) {
        pixelforge::brighten_channel(image, red, 20);
        benchmark::ClobberMemory();
    }
    report_bytes(state, image.size() / 3);
}

// What it costs to change layout: both copy every value into a new image.
void convert_to_planar(benchmark::State& state)
{
    const Image image(width, height);
    for (auto _ : state) {
        PlanarImage planar = pixelforge::to_planar(image);
        benchmark::DoNotOptimize(planar.data());
    }
    report_bytes(state, image.size());
}

void convert_to_interleaved(benchmark::State& state)
{
    const PlanarImage image(width, height);
    for (auto _ : state) {
        Image interleaved = pixelforge::to_interleaved(image);
        benchmark::DoNotOptimize(interleaved.data());
    }
    report_bytes(state, image.size());
}

} // namespace

BENCHMARK(grayscale_interleaved)->Unit(benchmark::kMillisecond);
BENCHMARK(grayscale_planar)->Unit(benchmark::kMillisecond);
BENCHMARK(brightness_interleaved)->Unit(benchmark::kMillisecond);
BENCHMARK(brightness_planar)->Unit(benchmark::kMillisecond);
BENCHMARK(brighten_red_interleaved)->Unit(benchmark::kMillisecond);
BENCHMARK(brighten_red_planar)->Unit(benchmark::kMillisecond);
BENCHMARK(convert_to_planar)->Unit(benchmark::kMillisecond);
BENCHMARK(convert_to_interleaved)->Unit(benchmark::kMillisecond);
