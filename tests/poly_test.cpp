// Tests for the four ways of choosing a filter in poly.hpp. All of them must
// give the pixels that the plain functions of filters.hpp give.

#include <gtest/gtest.h>

#include "pixelforge/filters.hpp"
#include "pixelforge/image.hpp"
#include "pixelforge/poly.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

using pixelforge::Image;
namespace poly = pixelforge::poly;

namespace {

Image make_test_image(int width, int height)
{
    Image image(width, height);
    for (std::size_t i = 0; i < image.size(); ++i) {
        image.data()[i] = static_cast<std::uint8_t>(i * 37 % 256);
    }
    return image;
}

std::vector<std::uint8_t> pixels(const Image& image)
{
    return {image.data(), image.data() + image.size()};
}

// brightness 40, then grayscale, then negative, with the plain functions.
Image expected_result(const Image& original)
{
    Image image = original;
    pixelforge::brightness(image, 40);
    pixelforge::grayscale(image);
    pixelforge::negative(image);
    return image;
}

} // namespace

// CRTP and variant filters can be copied; the virtual ones cannot (slicing).
static_assert(!std::is_copy_constructible_v<poly::VirtualFilter>);
static_assert(std::is_copy_constructible_v<poly::CrtpBrightness>);
static_assert(std::is_nothrow_move_constructible_v<poly::VariantFilter>);
// CRTP filters with different types have no common base class.
static_assert(!std::is_base_of_v<poly::CrtpFilter<poly::CrtpNegative>, poly::CrtpBrightness>);

// The three loops on their own, against the functions they repeat, for a range
// of settings, so that all four ways are known to start from the right arithmetic.
TEST(PolyOps, MatchThePlainFilters)
{
    const Image original = make_test_image(13, 7);
    for (const int amount : {-300, -100, 0, 25, 255, 1000}) {
        Image a = original, b = original;
        poly::BrightnessOp{amount}(a);
        pixelforge::brightness(b, amount);
        EXPECT_EQ(pixels(a), pixels(b)) << "brightness " << amount;
    }
    Image a = original, b = original;
    poly::NegativeOp{}(a);
    pixelforge::negative(b);
    EXPECT_EQ(pixels(a), pixels(b));

    a = original;
    b = original;
    poly::GrayscaleOp{}(a);
    pixelforge::grayscale(b);
    EXPECT_EQ(pixels(a), pixels(b));
}

TEST(Poly, VirtualChainGivesTheExpectedPixels)
{
    const Image original = make_test_image(11, 9);
    poly::VirtualChain chain;
    chain.push_back(std::make_unique<poly::VirtualBrightness>(40));
    chain.push_back(std::make_unique<poly::VirtualGrayscale>());
    chain.push_back(std::make_unique<poly::VirtualNegative>());

    Image image = original;
    poly::apply_virtual(chain, image);
    EXPECT_EQ(pixels(image), pixels(expected_result(original)));
}

TEST(Poly, CrtpChainGivesTheExpectedPixels)
{
    const Image original = make_test_image(11, 9);
    Image image = original;
    poly::apply_crtp(image, poly::CrtpBrightness(40), poly::CrtpGrayscale{},
                     poly::CrtpNegative{});
    EXPECT_EQ(pixels(image), pixels(expected_result(original)));
}

// A CRTP filter has the apply member that the v5 filter chain asks for, so the
// two chains can be mixed.
TEST(Poly, CrtpFiltersWorkInTheV5Chain)
{
    const Image original = make_test_image(11, 9);
    Image image = original;
    pixelforge::apply(image, poly::CrtpBrightness(40), poly::CrtpGrayscale{},
                      poly::CrtpNegative{});
    EXPECT_EQ(pixels(image), pixels(expected_result(original)));
}

TEST(Poly, VariantChainGivesTheExpectedPixels)
{
    const Image original = make_test_image(11, 9);
    const std::vector<poly::VariantFilter> chain = {
        poly::BrightnessOp{40}, poly::GrayscaleOp{}, poly::NegativeOp{}};

    Image image = original;
    poly::apply_variant(chain, image);
    EXPECT_EQ(pixels(image), pixels(expected_result(original)));
    EXPECT_TRUE(std::holds_alternative<poly::GrayscaleOp>(chain[1]));
}

TEST(Poly, FunctionChainGivesTheExpectedPixels)
{
    const Image original = make_test_image(11, 9);
    // Any callable fits: a function object, a lambda, or a function call.
    const std::vector<poly::FunctionFilter> chain = {
        poly::BrightnessOp{40},
        [](Image& image) { poly::GrayscaleOp{}(image); },
        std::bind(poly::NegativeOp{}, std::placeholders::_1),
    };

    Image image = original;
    poly::apply_function(chain, image);
    EXPECT_EQ(pixels(image), pixels(expected_result(original)));
}

TEST(Poly, EmptyChainsChangeNothing)
{
    const Image original = make_test_image(5, 3);
    Image image = original;
    poly::apply_virtual({}, image);
    poly::apply_variant({}, image);
    poly::apply_function({}, image);
    poly::apply_crtp(image);
    EXPECT_EQ(pixels(image), pixels(original));
}

TEST(Poly, EmptyStdFunctionThrows)
{
    Image image = make_test_image(2, 2);
    const std::vector<poly::FunctionFilter> chain = {poly::FunctionFilter{}};
    EXPECT_THROW(poly::apply_function(chain, image), std::bad_function_call);
}
