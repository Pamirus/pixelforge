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

// A chain of three filters on the 4K image (24.9 MB), three ways:
//   step_by_step_in_place   brightness(), negative(), brightness(): three passes
//   step_by_step_new_image  each step builds a new image from the last: three
//                           passes plus an allocation each (what "intermediate
//                           images" means)
//   lazy_one_pass           the expression template: one pass, no image between
#include "pixelforge/filters.hpp"
#include "pixelforge/lazy.hpp"

namespace {

void chain_step_by_step_in_place(benchmark::State& state)
{
    Image image(width, height);
    for (auto _ : state) {
        pixelforge::brightness(image, 90);
        pixelforge::negative(image);
        pixelforge::brightness(image, -40);
        benchmark::ClobberMemory();
    }
    state.SetBytesProcessed(state.iterations() * static_cast<std::int64_t>(image.size()));
}

void chain_step_by_step_new_image(benchmark::State& state)
{
    const Image source(width, height);
    for (auto _ : state) {
        Image a = source;
        pixelforge::brightness(a, 90);
        Image b = a;
        pixelforge::negative(b);
        Image c = b;
        pixelforge::brightness(c, -40);
        benchmark::DoNotOptimize(c.data());
    }
    state.SetBytesProcessed(state.iterations() * static_cast<std::int64_t>(source.size()));
}

void chain_lazy_one_pass(benchmark::State& state)
{
    const Image source(width, height);
    Image result(width, height);
    for (auto _ : state) {
        (pixelforge::lazy::from(source) | pixelforge::lazy::Brightness{90} |
         pixelforge::lazy::Negative{} | pixelforge::lazy::Brightness{-40})
            .assign_to(result);
        benchmark::ClobberMemory();
    }
    state.SetBytesProcessed(state.iterations() * static_cast<std::int64_t>(source.size()));
}

} // namespace

BENCHMARK(chain_step_by_step_in_place)->Unit(benchmark::kMillisecond);
BENCHMARK(chain_step_by_step_new_image)->Unit(benchmark::kMillisecond);
BENCHMARK(chain_lazy_one_pass)->Unit(benchmark::kMillisecond);
