#pragma once

// The decorator pattern: a Filter that wraps another Filter, has the same
// interface, and adds something around the call. Here it adds a ScopedTimer.
//
//     std::unique_ptr<Filter> blur = std::make_unique<BoxBlur>(3);
//     std::unique_ptr<Filter> timed = std::make_unique<TimedFilter>(std::move(blur));
//     timed->apply(image); // prints "blur 3: 74.12 ms" on std::cerr
//
// The code that applies filters does not know a filter is timed, and the
// filter inside does not know it is watched. Decorators can also be stacked,
// since a TimedFilter is itself a Filter.

#include "pixelforge/filters.hpp"
#include "pixelforge/scoped_timer.hpp"

#include <iostream>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <string>
#include <utility>

namespace pixelforge {

class TimedFilter : public Filter {
public:
    // Takes over the filter to wrap; throws std::invalid_argument if it is null.
    explicit TimedFilter(std::unique_ptr<Filter> inner, std::ostream& out = std::cerr)
        : inner_(std::move(inner)), out_(&out)
    {
        if (!inner_) {
            throw std::invalid_argument("TimedFilter needs a filter to wrap");
        }
    }

    // The timer is a local variable: it starts here and prints when apply
    // returns or throws.
    void apply(Image& image) const override
    {
        const ScopedTimer timer(inner_->description(), *out_);
        inner_->apply(image);
    }

    // Looks the same as the filter inside, so that tables and reports show
    // "blur 3", not "timed blur 3".
    std::string description() const override { return inner_->description(); }

private:
    std::unique_ptr<Filter> inner_;
    std::ostream* out_;
};

} // namespace pixelforge
