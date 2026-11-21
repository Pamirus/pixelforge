// One wrong call of the filter chain, chosen with -DCASE=1..4, to the
// std::enable_if version (-DVERSION_ENABLE_IF) or the concept version
// (default). compare.sh compiles every combination and prints the messages.
//
//     c++ -std=c++20 -Iinclude -fsyntax-only -DCASE=1 docs/error-messages/bad_chain.cpp

#include "pixelforge/blur.hpp"
#include "pixelforge/filters.hpp"

#ifdef VERSION_ENABLE_IF
#define CHAIN pixelforge::apply_enable_if
#else
#define CHAIN pixelforge::apply
#endif

struct NotAFilter {};

struct WrongSignature {
    void apply(int) const {}
};

struct NonConstApply {
    void apply(pixelforge::Image&) {}
};

void run(pixelforge::Image& image, pixelforge::FloatImage& floats)
{
#if CASE == 1
    CHAIN(image, pixelforge::Grayscale{}, NotAFilter{}); // not a filter at all
#elif CASE == 2
    CHAIN(image, WrongSignature{}); // apply takes an int, not an image
#elif CASE == 3
    CHAIN(image, NonConstApply{}); // apply is not const
#elif CASE == 4
    CHAIN(floats, pixelforge::Grayscale{}); // a filter class on a float image
#else
#error "give -DCASE=1, 2, 3 or 4"
#endif
}
