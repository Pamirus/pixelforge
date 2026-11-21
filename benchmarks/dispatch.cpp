// The four ways of choosing a filter from poly.hpp (virtual, CRTP, variant,
// function) on a chain of three filters, with Google Benchmark, and the effect
// of `final` on a virtual call.
//
//     build/benchmarks/pixelforge_benchmarks --benchmark_filter='chain|call'
//
// Each chain runs brightness, grayscale and negative. The first argument is the
// width and the second the height of the image: 16 x 9 pixels (432 bytes) is so
// small that the cost of finding the filter shows, and 3840 x 2160 is the usual
// 4K image, where the loops over 24.9 MB dominate. The pixels' values do not
// change the work, so a black image will do.

#include <benchmark/benchmark.h>

#include "pixelforge/image.hpp"
#include "pixelforge/poly.hpp"

#include <functional>
#include <memory>
#include <vector>

namespace {

namespace poly = pixelforge::poly;
using pixelforge::Image;

Image make_image(const benchmark::State& state)
{
    return Image(static_cast<int>(state.range(0)), static_cast<int>(state.range(1)));
}

// Nothing is chosen: the three loops called by name. What the others are
// measured against.
void chain_direct(benchmark::State& state)
{
    Image image = make_image(state);
    for (auto _ : state) {
        poly::BrightnessOp{40}(image);
        poly::GrayscaleOp{}(image);
        poly::NegativeOp{}(image);
        benchmark::ClobberMemory();
    }
}

void chain_virtual(benchmark::State& state)
{
    Image image = make_image(state);
    poly::VirtualChain chain;
    chain.push_back(std::make_unique<poly::VirtualBrightness>(40));
    chain.push_back(std::make_unique<poly::VirtualGrayscale>());
    chain.push_back(std::make_unique<poly::VirtualNegative>());
    for (auto _ : state) {
        poly::apply_virtual(chain, image);
        benchmark::ClobberMemory();
    }
}

void chain_crtp(benchmark::State& state)
{
    Image image = make_image(state);
    const poly::CrtpBrightness brightness(40);
    const poly::CrtpGrayscale grayscale;
    const poly::CrtpNegative negative;
    for (auto _ : state) {
        poly::apply_crtp(image, brightness, grayscale, negative);
        benchmark::ClobberMemory();
    }
}

void chain_variant(benchmark::State& state)
{
    Image image = make_image(state);
    const std::vector<poly::VariantFilter> chain = {poly::BrightnessOp{40}, poly::GrayscaleOp{},
                                                    poly::NegativeOp{}};
    for (auto _ : state) {
        poly::apply_variant(chain, image);
        benchmark::ClobberMemory();
    }
}

void chain_function(benchmark::State& state)
{
    Image image = make_image(state);
    const std::vector<poly::FunctionFilter> chain = {poly::BrightnessOp{40}, poly::GrayscaleOp{},
                                                     poly::NegativeOp{}};
    for (auto _ : state) {
        poly::apply_function(chain, image);
        benchmark::ClobberMemory();
    }
}

// One virtual call on a tiny image, through a pointer to the filter's own
// class. If the class can have subclasses the call goes through the vtable.
// If it is final, the compiler knows what the pointer can point to, and it
// calls the function directly and may inline it.
class OpenNegative : public poly::VirtualFilter {
public:
    void apply(Image& image) const override { poly::NegativeOp{}(image); }
};

class FinalNegative final : public poly::VirtualFilter {
public:
    void apply(Image& image) const override { poly::NegativeOp{}(image); }
};

template <typename Filter>
void call_through_pointer(benchmark::State& state)
{
    Image image = make_image(state);
    const Filter filter;
    const Filter* pointer = &filter;
    for (auto _ : state) {
        benchmark::DoNotOptimize(pointer); // hides what the pointer points to
        pointer->apply(image);
        benchmark::ClobberMemory();
    }
}

void both_sizes(::benchmark::Benchmark* b)
{
    b->Args({16, 9})->Args({3840, 2160});
}

void small_only(::benchmark::Benchmark* b)
{
    b->Args({16, 9});
}

} // namespace

BENCHMARK(chain_direct)->Apply(both_sizes)->Unit(benchmark::kNanosecond);
BENCHMARK(chain_virtual)->Apply(both_sizes)->Unit(benchmark::kNanosecond);
BENCHMARK(chain_crtp)->Apply(both_sizes)->Unit(benchmark::kNanosecond);
BENCHMARK(chain_variant)->Apply(both_sizes)->Unit(benchmark::kNanosecond);
BENCHMARK(chain_function)->Apply(both_sizes)->Unit(benchmark::kNanosecond);
BENCHMARK_TEMPLATE(call_through_pointer, OpenNegative)
    ->Apply(small_only)->Unit(benchmark::kNanosecond);
BENCHMARK_TEMPLATE(call_through_pointer, FinalNegative)
    ->Apply(small_only)->Unit(benchmark::kNanosecond);
