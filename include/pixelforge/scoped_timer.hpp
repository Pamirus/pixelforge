#pragma once

#include <chrono>
#include <format>
#include <iostream>
#include <ostream>
#include <string>
#include <utility>

namespace pixelforge {

// Prints how long the enclosing scope took, for example "grayscale: 6.21 ms":
//
//     {
//         ScopedTimer timer("grayscale");
//         grayscale(image);
//     } // prints here
//
// This is RAII applied to a clock instead of memory. The constructor starts
// the measurement and the destructor ends it, so the time is printed on every
// way out of the scope, including a return or an exception.
class ScopedTimer {
public:
    explicit ScopedTimer(std::string label, std::ostream& out = std::cerr)
        : label_(std::move(label)), out_(&out), start_(Clock::now())
    {
    }

    ~ScopedTimer()
    {
        const std::chrono::duration<double, std::milli> elapsed = Clock::now() - start_;
        *out_ << std::format("{}: {:.2f} ms\n", label_, elapsed.count());
    }

    // A copy would print the same measurement twice, so copying is disabled.
    // That also disables moving, which keeps the timer in its scope.
    ScopedTimer(const ScopedTimer&) = delete;
    ScopedTimer& operator=(const ScopedTimer&) = delete;

private:
    // steady_clock never jumps, unlike the wall clock, which can be adjusted
    // while the program runs.
    using Clock = std::chrono::steady_clock;

    std::string label_;
    std::ostream* out_;
    Clock::time_point start_;
};

} // namespace pixelforge
