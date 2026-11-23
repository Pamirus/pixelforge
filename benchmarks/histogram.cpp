// Gray-level histograms counted by 1 to 6 threads on a 3840 x 2160 image, in
// the ways of histogram.hpp. Two images:
//   noise  pseudo-random gray levels, like the spread of a photo
//   flat   one gray level everywhere: all threads want the same counter
// The racy version reports how many of the 8.3 million counts it lost.
//
//     build/benchmarks/pixelforge_benchmarks --benchmark_filter='histogram_'

#include <benchmark/benchmark.h>

#include "pixelforge/histogram.hpp"
#include "pixelforge/image.hpp"

#include <cstddef>
#include <cstdint>
#include <numeric>

namespace {

using pixelforge::Histogram;
using pixelforge::Image;

constexpr int width = 3840;
constexpr int height = 2160;
constexpr std::uint64_t pixels = std::uint64_t{width} * height;

const Image& test_image(bool flat)
{
    static const Image noise = [] {
        Image image(width, height);
        std::uint32_t state = 12345;
        for (std::size_t i = 0; i < image.size(); ++i) {
            state = state * 1664525u + 1013904223u;
            image.data()[i] = static_cast<std::uint8_t>(state >> 24);
        }
        return image;
    }();
    static const Image gray = [] {
        Image image(width, height);
        for (std::size_t i = 0; i < image.size(); ++i) {
            image.data()[i] = 100;
        }
        return image;
    }();
    return flat ? gray : noise;
}

// Args: threads, flat (0 or 1).
template <Histogram (*Count)(const Image&, unsigned)>
void run(benchmark::State& state)
{
    const Image& image = test_image(state.range(1) != 0);
    const auto threads = static_cast<unsigned>(state.range(0));
    Histogram last{};
    for (auto _ : state) {
        last = Count(image, threads);
        benchmark::DoNotOptimize(last);
    }
    const std::uint64_t counted = std::accumulate(last.begin(), last.end(), std::uint64_t{0});
    state.counters["lost"] = static_cast<double>(pixels - counted);
}

Histogram serial(const Image& image, unsigned) { return pixelforge::histogram_serial(image); }

void args(benchmark::Benchmark* b)
{
    for (const int flat : {0, 1}) {
        for (const int threads : {1, 2, 4, 6}) {
            b->Args({threads, flat});
        }
    }
    b->UseRealTime()->Unit(benchmark::kMillisecond);
}

} // namespace

BENCHMARK(run<serial>)->Name("histogram_serial")->Args({1, 0})->Args({1, 1})->UseRealTime()
    ->Unit(benchmark::kMillisecond);
BENCHMARK(run<pixelforge::histogram_racy>)->Name("histogram_racy")->Apply(args);
BENCHMARK(run<pixelforge::histogram_atomic>)->Name("histogram_atomic")->Apply(args);
BENCHMARK(run<pixelforge::histogram_per_thread>)->Name("histogram_per_thread")->Apply(args);
BENCHMARK(run<pixelforge::histogram_false_sharing>)->Name("histogram_false_sharing")->Apply(args);
