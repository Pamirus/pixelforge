// Not part of the build. One non-template function per variant, so that each
// one gets its own symbol in the object file and can be read with objdump.
// See docs/abstraction-assembly.md for the command.

#include "pixelforge/negative_variants.hpp"

#include <cstdint>

using pixelforge::Image;

void raw_loop(Image& image) { pixelforge::variants::negative_raw_loop(image); }
void transform(Image& image) { pixelforge::variants::negative_transform(image); }
void ranges(Image& image) { pixelforge::variants::negative_ranges(image); }
