// Tests for pixelforge::ScopedTimer.

#include <gtest/gtest.h>

#include "pixelforge/scoped_timer.hpp"

#include <chrono>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>

using pixelforge::ScopedTimer;

static_assert(!std::is_copy_constructible_v<ScopedTimer>);
static_assert(!std::is_move_constructible_v<ScopedTimer>);

TEST(ScopedTimer, PrintsLabelAndMillisecondsAtEndOfScope)
{
    std::ostringstream out;
    {
        const ScopedTimer timer("work", out);
        EXPECT_EQ(out.str(), ""); // nothing yet: the scope is still open
    }
    EXPECT_TRUE(std::regex_match(out.str(), std::regex(R"(work: \d+\.\d\d ms\n)"))) << out.str();
}

TEST(ScopedTimer, MeasuresAtLeastTheTimeSpent)
{
    std::ostringstream out;
    {
        const ScopedTimer timer("sleep", out);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    // sleep_for waits at least as long as asked, measured on steady_clock.
    const double ms = std::stod(out.str().substr(std::string("sleep: ").size()));
    EXPECT_GE(ms, 20.0) << out.str();
}

TEST(ScopedTimer, PrintsWhenTheScopeIsLeftByAnException)
{
    std::ostringstream out;
    try {
        const ScopedTimer timer("failing", out);
        throw std::runtime_error("stop");
    } catch (const std::runtime_error&) {
    }
    EXPECT_EQ(out.str().rfind("failing: ", 0), 0u) << out.str();
}

TEST(ScopedTimer, NestedTimersPrintInnerFirst)
{
    std::ostringstream out;
    {
        const ScopedTimer outer("outer", out);
        {
            const ScopedTimer inner("inner", out);
        }
    }
    const std::string text = out.str();
    EXPECT_LT(text.find("inner: "), text.find("outer: ")) << text;
}
