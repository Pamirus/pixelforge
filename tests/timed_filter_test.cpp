// TimedFilter: a decorator that times the filter it wraps.

#include <gtest/gtest.h>

#include "pixelforge/factory.hpp"
#include "pixelforge/filters.hpp"
#include "pixelforge/image.hpp"
#include "pixelforge/timed_filter.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <utility>

using namespace pixelforge;

namespace {

// A filter that throws, to see that the time is still printed.
class Failing : public Filter {
public:
    void apply(Image&) const override { throw std::runtime_error("failed"); }
    std::string description() const override { return "failing"; }
};

Image make_pattern()
{
    Image image(4, 3);
    for (std::size_t i = 0; i < image.size(); ++i) {
        image.data()[i] = static_cast<std::uint8_t>(i * 7);
    }
    return image;
}

} // namespace

TEST(TimedFilter, DoesTheSameAsTheFilterInside)
{
    std::ostringstream out;
    Image expected = make_pattern();
    Brightness(40).apply(expected);
    Image actual = make_pattern();
    TimedFilter(std::make_unique<Brightness>(40), out).apply(actual);
    for (std::size_t i = 0; i < actual.size(); ++i) {
        EXPECT_EQ(actual.data()[i], expected.data()[i]);
    }
}

TEST(TimedFilter, PrintsTheDescriptionAndATime)
{
    std::ostringstream out;
    Image image = make_pattern();
    TimedFilter(make_filter("blur", 2), out).apply(image);
    const std::string text = out.str();
    EXPECT_TRUE(text.starts_with("blur 2: ")) << text;
    EXPECT_TRUE(text.ends_with(" ms\n")) << text;
}

TEST(TimedFilter, KeepsTheDescriptionOfTheFilterInside)
{
    std::ostringstream out;
    EXPECT_EQ(TimedFilter(make_filter("brightness", 5), out).description(), "brightness 5");
}

TEST(TimedFilter, PrintsAlsoWhenTheFilterThrows)
{
    std::ostringstream out;
    Image image = make_pattern();
    const TimedFilter timed(std::make_unique<Failing>(), out);
    EXPECT_THROW(timed.apply(image), std::runtime_error);
    EXPECT_TRUE(out.str().starts_with("failing: ")) << out.str();
}

TEST(TimedFilter, CanBeStacked)
{
    std::ostringstream outer_out;
    std::ostringstream inner_out;
    Image image = make_pattern();
    auto inner = std::make_unique<TimedFilter>(make_filter("negative"), inner_out);
    TimedFilter(std::move(inner), outer_out).apply(image);
    EXPECT_FALSE(inner_out.str().empty());
    EXPECT_FALSE(outer_out.str().empty());
}

TEST(TimedFilter, RejectsANullFilter)
{
    EXPECT_THROW(TimedFilter(nullptr), std::invalid_argument);
}
