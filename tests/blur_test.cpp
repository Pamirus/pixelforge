// Tests for pixelforge::Blur<Size>, the box blur with its window size fixed at
// compile time, which must give exactly what box_blur gives with the same
// window, and for apply(image, filters...), the filter chain.

#include <gtest/gtest.h>

#include "pixelforge/blur.hpp"
#include "pixelforge/filters.hpp"
#include "pixelforge/image.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <type_traits>
#include <utility>
#include <vector>

using pixelforge::Blur;
using pixelforge::FloatImage;
using pixelforge::Image;

// The radius follows from the size, at compile time.
static_assert(Blur<3>::radius == 1);
static_assert(Blur<5>::radius == 2);
static_assert(Blur<21>::radius == 10);

namespace {

Image make_test_image(int width, int height)
{
    Image image(width, height);
    for (std::size_t i = 0; i < image.size(); ++i) {
        image.data()[i] = static_cast<std::uint8_t>(i * 37 % 256);
    }
    return image;
}

template <typename T>
std::vector<T> pixels(const pixelforge::BasicImage<T>& image)
{
    return {image.data(), image.data() + image.size()};
}

// Blurs copies of image with Blur<Size> and with box_blur, and compares them.
// The float sums are added in the same order, so even those match exactly.
template <int Size, typename T>
void expect_same_as_box_blur(const pixelforge::BasicImage<T>& image)
{
    pixelforge::BasicImage<T> fixed = image;
    Blur<Size>{}.apply(fixed);
    pixelforge::BasicImage<T> chosen_at_run_time = image;
    pixelforge::box_blur(chosen_at_run_time, Size / 2);
    EXPECT_EQ(pixels(fixed), pixels(chosen_at_run_time)) << "Blur<" << Size << ">";
}

} // namespace

TEST(Blur, MatchesTheBoxBlurOnEightBitPixels)
{
    const Image image = make_test_image(23, 14);
    expect_same_as_box_blur<3>(image);
    expect_same_as_box_blur<5>(image);
    expect_same_as_box_blur<11>(image);
}

TEST(Blur, MatchesTheBoxBlurOnFloatPixels)
{
    const FloatImage image = pixelforge::to_float(make_test_image(23, 14));
    expect_same_as_box_blur<3>(image);
    expect_same_as_box_blur<5>(image);
    expect_same_as_box_blur<11>(image);
}

// A window wider and taller than the image covers all of it from every pixel.
TEST(Blur, WindowLargerThanTheImageAveragesTheWholeImage)
{
    Image image(3, 1);
    const std::uint8_t channels[] = {0, 0, 0, 60, 60, 60, 90, 90, 90};
    std::copy(std::begin(channels), std::end(channels), image.data());
    Blur<21>{}.apply(image);
    EXPECT_EQ(pixels(image), std::vector<std::uint8_t>(9, 50));
}

TEST(Blur, EmptyImageStaysEmpty)
{
    Image image;
    Blur<3>{}.apply(image);
    EXPECT_TRUE(image.empty());
}

// apply(image, filters...) runs the filters left to right, exactly as calling
// them one after the other would.
TEST(FilterChain, MatchesCallingTheFiltersInOrder)
{
    const Image original = make_test_image(19, 11);

    Image chained = original;
    pixelforge::apply(chained, pixelforge::Grayscale{}, pixelforge::Brightness{20}, Blur<3>{},
                      pixelforge::Rotate90{});

    Image one_by_one = original;
    pixelforge::grayscale(one_by_one);
    pixelforge::brightness(one_by_one, 20);
    pixelforge::box_blur(one_by_one, 1);
    one_by_one = pixelforge::rotate_90(one_by_one);

    EXPECT_EQ(chained.width(), 11);
    EXPECT_EQ(pixels(chained), pixels(one_by_one));
}

// Brightening and then inverting is not the same as inverting and then
// brightening, so this shows the order is kept.
TEST(FilterChain, KeepsTheOrderOfTheFilters)
{
    const Image original = make_test_image(4, 3);
    Image first = original;
    pixelforge::apply(first, pixelforge::Brightness{100}, pixelforge::Negative{});
    Image second = original;
    pixelforge::apply(second, pixelforge::Negative{}, pixelforge::Brightness{100});
    EXPECT_NE(pixels(first), pixels(second));

    Image expected = original;
    pixelforge::brightness(expected, 100);
    pixelforge::negative(expected);
    EXPECT_EQ(pixels(first), pixels(expected));
}

TEST(FilterChain, WithNoFiltersChangesNothing)
{
    const Image original = make_test_image(5, 2);
    Image image = original;
    pixelforge::apply(image);
    EXPECT_EQ(pixels(image), pixels(original));
}

// The filter classes take an Image, but Blur<Size> takes either pixel type,
// so a chain of blurs works on a FloatImage too.
TEST(FilterChain, WorksOnFloatImagesWithBlurs)
{
    const FloatImage original = pixelforge::to_float(make_test_image(9, 7));
    FloatImage chained = original;
    pixelforge::apply(chained, Blur<3>{}, Blur<5>{});

    FloatImage one_by_one = original;
    pixelforge::box_blur(one_by_one, 1);
    pixelforge::box_blur(one_by_one, 2);
    EXPECT_EQ(pixels(chained), pixels(one_by_one));
}

// Which types count as filters is decided at compile time, so these are
// static_asserts. The filter classes work on 8-bit images, Blur<Size> on both.
// Each check is made for the enable_if version (is_filter_for, apply_enable_if)
// and for the concept version (FilterFor, apply), and the two must agree.
namespace {

struct NotAFilter {};

struct WrongSignature {
    void apply(int) const {}
};

// Only a non-const apply: a chain holds its filters by const reference.
struct NonConstApply {
    void apply(Image&) {}
};

// The two chains as function objects, so that the same check can be run on
// both. The decltype in the return type makes a bad call a substitution
// failure here, not an error.
struct CallApply {
    template <typename I, typename... F>
    auto operator()(I& image, const F&... filters) const
        -> decltype(pixelforge::apply(image, filters...));
};

struct CallApplyEnableIf {
    template <typename I, typename... F>
    auto operator()(I& image, const F&... filters) const
        -> decltype(pixelforge::apply_enable_if(image, filters...));
};

template <typename Void, typename Call, typename T, typename... Filters>
struct can_call_impl : std::false_type {};

template <typename Call, typename T, typename... Filters>
struct can_call_impl<std::void_t<decltype(std::declval<Call>()(
                         std::declval<pixelforge::BasicImage<T>&>(),
                         std::declval<const Filters&>()...))>,
                     Call, T, Filters...> : std::true_type {};

// Whether the call compiles for an image of this type and these filters.
template <typename Call, typename T, typename... Filters>
constexpr bool can_call = can_call_impl<void, Call, T, Filters...>::value;

// True if both versions give the same answer, and that answer is expected.
template <bool expected, typename T, typename... Filters>
constexpr bool both_say = can_call<CallApply, T, Filters...> == expected &&
                          can_call<CallApplyEnableIf, T, Filters...> == expected;

// The concept and the trait on one filter type.
template <typename F, typename T>
constexpr bool agree = pixelforge::FilterFor<F, T> == pixelforge::detail::is_filter_for_v<F, T>;

} // namespace

using pixelforge::FilterFor;

static_assert(FilterFor<pixelforge::Grayscale, std::uint8_t>);
static_assert(FilterFor<Blur<3>, std::uint8_t>);
static_assert(FilterFor<Blur<3>, float>);
static_assert(!FilterFor<pixelforge::Grayscale, float>);
static_assert(!FilterFor<NotAFilter, std::uint8_t>);
static_assert(!FilterFor<WrongSignature, std::uint8_t>);
static_assert(!FilterFor<NonConstApply, std::uint8_t>);
static_assert(!FilterFor<int, std::uint8_t>);

static_assert(agree<pixelforge::Grayscale, std::uint8_t>);
static_assert(agree<pixelforge::Grayscale, float>);
static_assert(agree<Blur<3>, float>);
static_assert(agree<NotAFilter, std::uint8_t>);
static_assert(agree<WrongSignature, std::uint8_t>);
static_assert(agree<NonConstApply, std::uint8_t>);
static_assert(agree<int, std::uint8_t>);

static_assert(both_say<true, std::uint8_t>); // an empty chain
static_assert(both_say<true, std::uint8_t, pixelforge::Grayscale, Blur<3>>);
static_assert(both_say<true, float, Blur<3>, Blur<5>>);
static_assert(both_say<false, float, pixelforge::Grayscale>);
static_assert(both_say<false, std::uint8_t, NotAFilter>);
static_assert(both_say<false, std::uint8_t, pixelforge::Grayscale, NotAFilter, Blur<3>>);
static_assert(both_say<false, std::uint8_t, NonConstApply>);

TEST(FilterChain, ConstraintsAreCheckedWhileCompiling)
{
    // The checks above run when this file is compiled; this test only makes
    // sure the file and its static_asserts are part of the build.
    SUCCEED();
}

TEST(FilterChain, EnableIfAndConceptVersionsGiveTheSameImage)
{
    const Image original = make_test_image(17, 9);
    Image with_concept = original;
    pixelforge::apply(with_concept, pixelforge::Grayscale{}, Blur<3>{}, pixelforge::Negative{});
    Image with_enable_if = original;
    pixelforge::apply_enable_if(with_enable_if, pixelforge::Grayscale{}, Blur<3>{},
                                pixelforge::Negative{});
    EXPECT_EQ(pixels(with_concept), pixels(with_enable_if));
}
