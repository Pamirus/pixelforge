#include "pixelforge/factory.hpp"

#include <array>
#include <format>
#include <stdexcept>
#include <string>

namespace pixelforge {

namespace {

// A function that builds a filter of type F, with no setting or with one. The
// table holds plain function pointers, so no row needs a lambda of its own.
template <typename F>
std::unique_ptr<Filter> make_plain(int)
{
    return std::make_unique<F>();
}

template <typename F>
std::unique_ptr<Filter> make_with_number(int setting)
{
    return std::make_unique<F>(setting);
}

constexpr std::array<FilterSpec, 10> specs{{
    {"grayscale", false, 0, 0, make_plain<Grayscale>},
    {"brightness", true, -255, 255, make_with_number<Brightness>},
    {"negative", false, 0, 0, make_plain<Negative>},
    {"threshold", true, 0, 255, make_with_number<Threshold>},
    {"flip-horizontal", false, 0, 0, make_plain<FlipHorizontal>},
    {"flip-vertical", false, 0, 0, make_plain<FlipVertical>},
    {"rotate", false, 0, 0, make_plain<Rotate90>},
    {"blur", true, 1, 10, make_with_number<BoxBlur>},
    {"sharpen", false, 0, 0, make_plain<Sharpen>},
    {"sobel", false, 0, 0, make_plain<Sobel>},
}};

} // namespace

std::span<const FilterSpec> filter_specs()
{
    return specs;
}

const FilterSpec* find_filter(std::string_view name)
{
    for (const FilterSpec& spec : specs) {
        if (spec.name == name) {
            return &spec;
        }
    }
    return nullptr;
}

std::unique_ptr<Filter> make_filter(std::string_view name, std::optional<int> setting)
{
    const FilterSpec* spec = find_filter(name);
    if (spec == nullptr) {
        throw std::invalid_argument(std::format("unknown filter '{}'", name));
    }
    if (!spec->takes_number) {
        if (setting) {
            throw std::invalid_argument(std::format("{} does not take a number", name));
        }
        return spec->make(0);
    }
    if (!setting || *setting < spec->min || *setting > spec->max) {
        throw std::invalid_argument(
            std::format("{} needs a whole number from {} to {}", name, spec->min, spec->max));
    }
    return spec->make(*setting);
}

} // namespace pixelforge
