// Copying versus moving a 3840 x 2160 image (24.9 MB), with Google Benchmark.
//
//     build/benchmarks/pixelforge_benchmarks
//
// The images are CountedImages, and each benchmark reports its copies and
// moves per iteration, so the output shows that it does what its name says.
// The counting itself adds one increment per copy or move.

#include <benchmark/benchmark.h>

#include "pixelforge/counted_image.hpp"

#include <cstdint>
#include <utility>

namespace {

using pixelforge::CountedImage;

constexpr int width = 3840;
constexpr int height = 2160;

void report_counts(benchmark::State& state)
{
    const auto counts = CountedImage::counts();
    state.counters["copies"] =
        benchmark::Counter(counts.copies(), benchmark::Counter::kAvgIterations);
    state.counters["moves"] =
        benchmark::Counter(counts.moves(), benchmark::Counter::kAvgIterations);
}

void report_bytes_copied(benchmark::State& state, const CountedImage& image)
{
    state.SetBytesProcessed(state.iterations() *
                            static_cast<std::int64_t>(image.image().size()));
}

// A copy into a new image: allocate 24.9 MB, copy the pixels, free the copy.
void copy_into_new_image(benchmark::State& state)
{
    const CountedImage source(width, height);
    CountedImage::reset_counts();
    for (auto _ : state) {
        CountedImage copy(source);
        benchmark::DoNotOptimize(copy.image().data());
    }
    report_bytes_copied(state, source);
    report_counts(state);
}

// A copy into an image of the same size. The vector keeps its buffer, so only
// the pixels are copied and nothing is allocated.
void copy_into_existing_image(benchmark::State& state)
{
    const CountedImage source(width, height);
    CountedImage target(width, height);
    CountedImage::reset_counts();
    for (auto _ : state) {
        target = source;
        benchmark::DoNotOptimize(target.image().data());
        benchmark::ClobberMemory();
    }
    report_bytes_copied(state, source);
    report_counts(state);
}

// A move into a new image and back again, so every iteration starts the same.
// Only pointers and sizes change hands; the 24.9 MB stay where they are.
void move_and_move_back(benchmark::State& state)
{
    CountedImage image(width, height);
    CountedImage::reset_counts();
    for (auto _ : state) {
        CountedImage moved(std::move(image));
        benchmark::DoNotOptimize(moved.image().data());
        image = std::move(moved);
    }
    report_counts(state);
}

} // namespace

BENCHMARK(copy_into_new_image)->Unit(benchmark::kMillisecond);
BENCHMARK(copy_into_existing_image)->Unit(benchmark::kMillisecond);
BENCHMARK(move_and_move_back)->Unit(benchmark::kNanosecond);
