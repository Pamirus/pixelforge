#pragma once

// A histogram of the gray levels of an 8-bit image, counted in parallel in
// five ways (stage v10). entry v of the result is how many pixels have the
// gray level v, the rounded average of their three channels, as in grayscale().
//
// The easy way to split the counting is the one that goes wrong, and the point
// of the stage is to see how, and what each repair costs:
//
//   histogram_serial       one thread; the reference
//   histogram_racy         all threads add to the same 256 counters with a
//                          plain `++`. That is a DATA RACE: two threads read the
//                          same count, both add one, both write; one addition is
//                          lost. In C++ it is undefined behaviour. It exists
//                          only to show the loss and what a repair costs; the
//                          tests do not call it.
//   histogram_atomic       the same shared counters, but each `++` is one atomic
//                          operation, so none is lost. Threads still queue up for
//                          the same counter when they hit the same gray level.
//   histogram_per_thread   every thread counts into its own 256 counters, and
//                          the counters are added together at the end. Nothing
//                          is shared while counting.
//   histogram_false_sharing  also private counters per thread, but laid out
//                          badly: the counters of the same gray level for all
//                          threads sit next to each other. No counter is shared,
//                          but a cache line (64 bytes) holds counters of several
//                          threads, and when one thread writes the line, the
//                          others' copies are invalidated. That is false sharing.

#include "pixelforge/image.hpp"

#include <array>
#include <cstdint>

namespace pixelforge {

using Histogram = std::array<std::uint64_t, 256>;

Histogram histogram_serial(const Image& image);
Histogram histogram_racy(const Image& image, unsigned threads);
Histogram histogram_atomic(const Image& image, unsigned threads);
Histogram histogram_per_thread(const Image& image, unsigned threads);
Histogram histogram_false_sharing(const Image& image, unsigned threads);

} // namespace pixelforge
