#pragma once

// A factory: builds a filter from its name, such as "blur", and a setting.
//
// Before, main.cpp had one `else if (arg == "--blur")` branch for every
// filter. Now there is one table, and adding a filter means adding one row:
// the name, whether it takes a number, which numbers are allowed, and how to
// build it. The command line, the tests and any other caller go through the
// same table.

#include "pixelforge/filters.hpp"

#include <memory>
#include <optional>
#include <span>
#include <string_view>

namespace pixelforge {

struct FilterSpec {
    std::string_view name;      // without the leading "--", e.g. "blur"
    bool takes_number;          // does it need a setting, as in "--blur 3"?
    int min;                    // the allowed settings, only if takes_number
    int max;
    std::unique_ptr<Filter> (*make)(int setting);
};

// Every filter the factory knows, in the order of the command line help.
std::span<const FilterSpec> filter_specs();

// The row for a name, or nullptr if there is no such filter.
const FilterSpec* find_filter(std::string_view name);

// Builds the filter called `name`. Throws std::invalid_argument if the name is
// unknown, if a number is missing or given to a filter that takes none, or if
// the number is out of range.
std::unique_ptr<Filter> make_filter(std::string_view name,
                                    std::optional<int> setting = std::nullopt);

} // namespace pixelforge
