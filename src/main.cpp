// pixelforge: the C++ command line.
//
//     pixelforge in.png out.png --grayscale --brightness 20
//
// Filters run in the order they are given. Errors are exceptions caught once
// in main, where the C baseline needs a goto to a single cleanup label.

#include "pixelforge/filters.hpp"
#include "pixelforge/image.hpp"
#include "pixelforge/io.hpp"
#include "pixelforge/scoped_timer.hpp"

#include <charconv>
#include <cstddef>
#include <exception>
#include <format>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {

using pixelforge::Image;

constexpr std::string_view usage =
    "usage: pixelforge INPUT OUTPUT [OPTION]...\n"
    "\n"
    "Filters, applied in the order given:\n"
    "  --grayscale       replace R, G and B with their average\n"
    "  --brightness N    add N (-255..255) to every channel\n"
    "  --negative        replace every channel value v with 255 - v\n"
    "  --threshold N     white where the gray level is at least N (0..255),\n"
    "                    black elsewhere\n"
    "  --flip-horizontal mirror left to right\n"
    "  --flip-vertical   mirror top to bottom\n"
    "  --rotate          turn 90 degrees clockwise (repeat for 180 or 270)\n"
    "  --blur N          box blur: average each pixel with the pixels up to N\n"
    "                    (1..10) pixels away\n"
    "  --sharpen         make edges and details crisper\n"
    "  --sobel           edge detection: bright where the gray level changes fast\n"
    "\n"
    "Other options:\n"
    "  --time            print how long loading, each filter and saving take\n"
    "\n"
    "OUTPUT must end in .png, .jpg, .jpeg or .bmp.\n";

// A mistake in the arguments. main prints it and returns 2.
class UsageError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// The filters are of different types, so the list holds pointers to their
// common base class. Each unique_ptr owns its filter and deletes it when the
// list goes away; Options can therefore be moved but not copied.
struct Options {
    std::string input;
    std::string output;
    std::vector<std::unique_ptr<pixelforge::Filter>> filters;
    bool time = false;
};

// A whole decimal number from min to max, or nothing.
std::optional<int> parse_int(std::string_view text, int min, int max)
{
    if (text.starts_with('+') && !text.substr(1).starts_with('-')) {
        text.remove_prefix(1); // from_chars does not accept a leading +
    }
    int value = 0;
    const char* end = text.data() + text.size();
    const auto [stop, error] = std::from_chars(text.data(), end, value);
    if (error != std::errc{} || stop != end || value < min || value > max) {
        return std::nullopt;
    }
    return value;
}

// Every argument is checked before anything is loaded, so a typo fails fast.
Options parse_arguments(const std::vector<std::string_view>& args)
{
    Options options{std::string(args[0]), std::string(args[1]), {}, false};

    for (std::size_t i = 2; i < args.size(); ++i) {
        const std::string_view arg = args[i];
        // Reads the number after the current option and steps over it.
        const auto number_after = [&](int min, int max) {
            const std::optional<int> value =
                i + 1 < args.size() ? parse_int(args[i + 1], min, max) : std::nullopt;
            if (!value) {
                throw UsageError(
                    std::format("{} needs a whole number from {} to {}", arg, min, max));
            }
            ++i;
            return *value;
        };

        if (arg == "--grayscale") {
            options.filters.push_back(std::make_unique<pixelforge::Grayscale>());
        } else if (arg == "--brightness") {
            options.filters.push_back(
                std::make_unique<pixelforge::Brightness>(number_after(-255, 255)));
        } else if (arg == "--negative") {
            options.filters.push_back(std::make_unique<pixelforge::Negative>());
        } else if (arg == "--threshold") {
            options.filters.push_back(
                std::make_unique<pixelforge::Threshold>(number_after(0, 255)));
        } else if (arg == "--flip-horizontal") {
            options.filters.push_back(std::make_unique<pixelforge::FlipHorizontal>());
        } else if (arg == "--flip-vertical") {
            options.filters.push_back(std::make_unique<pixelforge::FlipVertical>());
        } else if (arg == "--rotate") {
            options.filters.push_back(std::make_unique<pixelforge::Rotate90>());
        } else if (arg == "--blur") {
            options.filters.push_back(std::make_unique<pixelforge::BoxBlur>(number_after(1, 10)));
        } else if (arg == "--sharpen") {
            options.filters.push_back(std::make_unique<pixelforge::Sharpen>());
        } else if (arg == "--sobel") {
            options.filters.push_back(std::make_unique<pixelforge::Sobel>());
        } else if (arg == "--time") {
            options.time = true;
        } else {
            throw UsageError(std::format("unknown option '{}'", arg));
        }
    }
    return options;
}

// Runs work, timed by a ScopedTimer if timing is on. emplace builds the timer
// inside the optional, so the timer does not have to be movable.
template <typename Work>
void timed(bool enabled, const std::string& label, Work work)
{
    std::optional<pixelforge::ScopedTimer> timer;
    if (enabled) {
        timer.emplace(label);
    }
    work();
}

void run(const Options& options)
{
    Image image;
    timed(options.time, "load", [&] { image = pixelforge::load(options.input); });
    for (const auto& filter : options.filters) {
        timed(options.time, filter->description(), [&] { filter->apply(image); });
    }
    timed(options.time, "save", [&] { pixelforge::save(image, options.output); });
}

} // namespace

int main(int argc, char** argv)
{
    try {
        const std::vector<std::string_view> args(argv + 1, argv + argc);
        if (args.size() == 1 && (args[0] == "--help" || args[0] == "-h")) {
            std::cout << usage;
            return 0;
        }
        if (args.size() < 2) {
            std::cerr << usage;
            return 2;
        }
        run(parse_arguments(args));
        return 0;
    } catch (const UsageError& error) {
        std::cerr << "pixelforge: " << error.what() << "\nTry 'pixelforge --help'.\n";
        return 2;
    } catch (const std::exception& error) {
        std::cerr << "pixelforge: " << error.what() << '\n';
        return 1;
    }
}
