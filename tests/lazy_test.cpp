// Lazy filter chains must give the same pixels as applying the filters one
// after the other.

#include <gtest/gtest.h>

#include "pixelforge/filters.hpp"
#include "pixelforge/image.hpp"
#include "pixelforge/lazy.hpp"

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <type_traits>

using namespace pixelforge;

namespace {

template <typename T>
BasicImage<T> make_pattern(int width, int height)
{
    BasicImage<T> image(width, height);
    for (std::size_t i = 0; i < image.size(); ++i) {
        const auto level = static_cast<unsigned>((i * 53 + 7) % 256);
        if constexpr (std::is_same_v<T, std::uint8_t>) {
            image.data()[i] = static_cast<T>(level);
        } else {
            image.data()[i] = static_cast<T>(level) / 255.0f;
        }
    }
    return image;
}

template <typename T>
void expect_equal(const BasicImage<T>& a, const BasicImage<T>& b)
{
    ASSERT_EQ(a.size(), b.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        ASSERT_EQ(a.data()[i], b.data()[i]) << "at " << i;
    }
}

template <typename T>
void check_chain_of_three()
{
    const BasicImage<T> original = make_pattern<T>(33, 17);

    BasicImage<T> eager = original;
    brightness(eager, 90);
    negative(eager);
    brightness(eager, -40);

    const BasicImage<T> lazy_result =
        (lazy::from(original) | lazy::Brightness{90} | lazy::Negative{} | lazy::Brightness{-40})
            .evaluate();
    expect_equal(lazy_result, eager);
}

} // namespace

TEST(Lazy, ChainOfThreeMatchesTheStepByStepResult8Bit)
{
    check_chain_of_three<std::uint8_t>();
}

TEST(Lazy, ChainOfThreeMatchesTheStepByStepResultFloat)
{
    check_chain_of_three<float>();
}

TEST(Lazy, AChainWithNoFiltersCopiesTheImage)
{
    const Image original = make_pattern<std::uint8_t>(5, 3);
    expect_equal(lazy::from(original).evaluate(), original);
}

TEST(Lazy, ClampingHappensAtEveryStepNotOnlyAtTheEnd)
{
    // 200 + 100 clamps to 255 and the negative of that is 0. Without the
    // clamp in the middle it would be 255 - 300, which is not a channel value.
    Image original(1, 1);
    original.data()[0] = 200;
    const Image result =
        (lazy::from(original) | lazy::Brightness{100} | lazy::Negative{}).evaluate();
    EXPECT_EQ(result.data()[0], 0);
}

TEST(Lazy, BuildingAChainDoesNotChangeTheSource)
{
    const Image original = make_pattern<std::uint8_t>(4, 4);
    const Image copy = original;
    const auto chain = lazy::from(original) | lazy::Negative{};
    (void)chain;
    expect_equal(original, copy);
}

TEST(Lazy, CanBeAssignedInPlace)
{
    Image image = make_pattern<std::uint8_t>(9, 7);
    Image expected = image;
    negative(expected);
    brightness(expected, 20);
    (lazy::from(image) | lazy::Negative{} | lazy::Brightness{20}).assign_to(image);
    expect_equal(image, expected);
}

TEST(Lazy, ResultOfTheWrongSizeIsRejected)
{
    const Image source(4, 4);
    Image wrong(3, 4);
    EXPECT_THROW(lazy::from(source).assign_to(wrong), std::invalid_argument);
}

TEST(Lazy, TheTypeOfAChainListsItsFilters)
{
    const Image image(2, 2);
    const auto chain = lazy::from(image) | lazy::Negative{} | lazy::Brightness{5};
    using Expected = const lazy::Chain<std::uint8_t, lazy::Negative, lazy::Brightness>;
    static_assert(std::is_same_v<decltype(chain), Expected>);
    (void)chain;
}
