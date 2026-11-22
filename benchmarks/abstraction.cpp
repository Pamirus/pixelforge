// The negative written three ways (raw loop, std::transform with a lambda,
// std::ranges with a view) on a 3840 x 2160 image (24.9 MB), with Google
// Benchmark.
//
//     build/benchmarks/pixelforge_benchmarks --benchmark_filter='negative_'

#include <benchmark/benchmark.h>

#include "pixelforge/image.hpp"
#include "pixelforge/negative_variants.hpp"

#include <cstdint>

namespace {

using pixelforge::Image;

constexpr int width = 3840;
constexpr int height = 2160;

template <void (*Filter)(Image&)>
void run(benchmark::State& state)
{
    Image image(width, height);
    for (auto _ : state) {
        Filter(image);
        benchmark::ClobberMemory();
    }
    state.SetBytesProcessed(state.iterations() * static_cast<std::int64_t>(image.size()));
}

} // namespace

BENCHMARK(run<pixelforge::variants::negative_raw_loop<std::uint8_t>>)
    ->Name("negative_raw_loop")->Unit(benchmark::kMillisecond);
BENCHMARK(run<pixelforge::variants::negative_transform<std::uint8_t>>)
    ->Name("negative_transform")->Unit(benchmark::kMillisecond);
BENCHMARK(run<pixelforge::variants::negative_ranges<std::uint8_t>>)
    ->Name("negative_ranges")->Unit(benchmark::kMillisecond);
