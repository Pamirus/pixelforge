// The factory builds filters from the names of the command line.

#include <gtest/gtest.h>

#include "pixelforge/factory.hpp"
#include "pixelforge/filters.hpp"
#include "pixelforge/image.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>

using namespace pixelforge;

TEST(Factory, BuildsAFilterWithoutANumber)
{
    const auto filter = make_filter("negative");
    EXPECT_EQ(filter->description(), "negative");
}

TEST(Factory, BuildsAFilterWithANumber)
{
    EXPECT_EQ(make_filter("blur", 3)->description(), "blur 3");
    EXPECT_EQ(make_filter("brightness", -20)->description(), "brightness -20");
    EXPECT_EQ(make_filter("threshold", 128)->description(), "threshold 128");
}

TEST(Factory, EveryRowBuildsAFilterOfItsName)
{
    // Each filter's description starts with its name (the dashes of the
    // command line are spaces in descriptions, and "rotate" says "rotate 90").
    for (const FilterSpec& spec : filter_specs()) {
        const auto filter = make_filter(spec.name, spec.takes_number ? std::optional<int>(spec.min)
                                                                      : std::nullopt);
        std::string name(spec.name);
        for (char& c : name) {
            if (c == '-') {
                c = ' ';
            }
        }
        EXPECT_EQ(filter->description().substr(0, name.size()), name) << spec.name;
    }
}

TEST(Factory, TheBuiltFilterDoesTheSameAsTheClass)
{
    Image a(3, 2);
    for (std::size_t i = 0; i < a.size(); ++i) {
        a.data()[i] = static_cast<std::uint8_t>(i * 40);
    }
    Image b = a;
    make_filter("brightness", 50)->apply(a);
    Brightness(50).apply(b);
    for (std::size_t i = 0; i < a.size(); ++i) {
        EXPECT_EQ(a.data()[i], b.data()[i]);
    }
}

TEST(Factory, UnknownNameThrows)
{
    EXPECT_THROW(make_filter("sepia"), std::invalid_argument);
    EXPECT_EQ(find_filter("sepia"), nullptr);
}

TEST(Factory, MissingNumberThrows)
{
    EXPECT_THROW(make_filter("blur"), std::invalid_argument);
}

TEST(Factory, NumberOutOfRangeThrows)
{
    EXPECT_THROW(make_filter("blur", 0), std::invalid_argument);
    EXPECT_THROW(make_filter("blur", 11), std::invalid_argument);
    EXPECT_THROW(make_filter("brightness", 256), std::invalid_argument);
    EXPECT_NO_THROW(make_filter("brightness", -255));
}

TEST(Factory, NumberForAFilterWithoutOneThrows)
{
    EXPECT_THROW(make_filter("negative", 3), std::invalid_argument);
}

TEST(Factory, NamesAreUnique)
{
    for (const FilterSpec& a : filter_specs()) {
        int same = 0;
        for (const FilterSpec& b : filter_specs()) {
            same += a.name == b.name;
        }
        EXPECT_EQ(same, 1) << a.name;
    }
}
