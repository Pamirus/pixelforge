// pixelforge_bench: times the C++ filters on one image, the same way
// baseline_c/bench.c times the C ones, so the two outputs can be compared.
//
//     pixelforge_bench images/cat-4k.jpg [RUNS]
//
// Before every run the original pixels are copied back, outside the timed
// part, so each run filters the same data. The median and the fastest run
// are printed.

#include "pixelforge/filters.hpp"
#include "pixelforge/image.hpp"
#include "pixelforge/io.hpp"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <exception>
#include <format>
#include <iostream>
#include <system_error>
#include <vector>

namespace {

using pixelforge::Image;
using Clock = std::chrono::steady_clock;

template <typename Filter>
void time_filter(const char* name, Filter filter, const Image& source, Image& work,
                 std::vector<double>& times)
{
    for (double& time : times) {
        std::copy_n(source.data(), source.size(), work.data());
        const auto start = Clock::now();
        filter(work);
        time = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    }

    std::sort(times.begin(), times.end());
    const std::size_t n = times.size();
    const double median = n % 2 != 0 ? times[n / 2] : (times[n / 2 - 1] + times[n / 2]) / 2.0;
    // Image bytes per second; the filter itself reads and writes each byte.
    const double gb_per_s = static_cast<double>(source.size()) / (median * 1e6);
    std::cout << std::format("{:<12} median {:8.3f} ms   fastest {:8.3f} ms   {:6.1f} GB/s\n",
                             name, median, times.front(), gb_per_s);
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 2 || argc > 3) {
        std::cerr << "usage: pixelforge_bench IMAGE [RUNS]\n";
        return 2;
    }
    int runs = 30;
    if (argc == 3) {
        const char* end = argv[2] + std::strlen(argv[2]);
        const auto [stop, error] = std::from_chars(argv[2], end, runs);
        if (error != std::errc{} || stop != end || runs < 1 || runs > 100000) {
            std::cerr << "pixelforge_bench: RUNS must be from 1 to 100000\n";
            return 2;
        }
    }

    try {
        const Image source = pixelforge::load(argv[1]);
        Image work = source;
        std::vector<double> times(static_cast<std::size_t>(runs));

        std::cout << std::format("{}: {} x {}, {:.1f} MB, {} runs per filter\n", argv[1],
                                 source.width(), source.height(),
                                 static_cast<double>(source.size()) / 1e6, runs);
        time_filter("grayscale", [](Image& image) { pixelforge::grayscale(image); }, source,
                    work, times);
        time_filter("brightness", [](Image& image) { pixelforge::brightness(image, 20); },
                    source, work, times);
    } catch (const std::exception& error) {
        std::cerr << "pixelforge_bench: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
