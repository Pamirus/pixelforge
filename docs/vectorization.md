# What the compiler says about vectorization

Measured and read on 2026-11-22 with Apple Clang 21 (arm64, NEON), flags of
the Release build (`-O3 -DNDEBUG`). Only Clang was asked: GCC's
`-fopt-info-vec` was not run for this note, so nothing here is claimed about
GCC.

Reproduce:

    clang++ -std=c++20 -O3 -DNDEBUG -Iinclude -c src/filters.cpp -o /dev/null \
        -Rpass=loop-vectorize -Rpass-missed=loop-vectorize \
        -Rpass-analysis=loop-vectorize

`-Rpass` prints the loops that were vectorized, `-Rpass-missed` the ones that
were not, `-Rpass-analysis` the reason, when the compiler gives one.

## Interleaved grayscale: "uncountable loop"

    for (std::size_t i = 0; i < size; i += 3) { ... }
    -> loop not vectorized: Cannot vectorize uncountable loop
    -> could not determine number of loop iterations

The loop steps by three. If `size` were close to the largest `size_t`,
`i += 3` could wrap around past zero and the loop would never end, so the
compiler cannot prove how many iterations there are and gives up. This is
about the loop *shape*, not about the memory layout.

To check that, I compiled two small functions on their own (not part of the
library): the same arithmetic with `i += 3`, and with a pixel counter `n` and
`p + 3 * n`. The first is reported as uncountable, the second is vectorized
(width 16). So the explanation is confirmed for this loop. Whether the
counting version is also faster was not measured, and it still has to
de-interleave three channels, so it will not reach the planar version.
`brighten_channel` (stride 3 as well) is rejected for the same reason.

## Planar grayscale and the other planar loops

Vectorized, width 16 (sixteen 8-bit values per NEON register): grayscale,
brightness, `brighten_channel` and both conversion loops. The three planes are plain
arrays, one value per plane per pixel, so the compiler does not have to
shuffle (the conversions interleave and de-interleave with NEON's
structured loads and stores). This agrees with the 2.4x seen in the layout benchmark (#38), and it
is a likely, not an isolated, cause of it. The planar `brighten_channel`
(one contiguous plane) is vectorized as well.

## Flips

- `flip_horizontal`: the loop over `left` / `right` is reported as not
  vectorized, without a reason. The data moves backwards in `right` and the
  body is a 3-byte swap; the earlier assembly check (#39) showed no vector
  instruction in it, and 12 byte operations per pixel swap.
- `flip_horizontal_blocked`: the block loop is rejected with "instruction
  cannot be vectorized". My reading, not confirmed: it contains calls
  (`copy_n` into stack buffers) and a reversed inner loop. The tail loop is
  rejected like the plain flip. The blocked version is only ~1.4x faster.
- `flip_vertical` swaps whole rows with `swap_ranges`; libc++'s loop in
  `swap_ranges.h` is vectorized (width 16), which fits the vertical flip being
  about as fast as a plain copy.

So the flips differ because of the work per byte, not only the memory access
pattern: reversing the pixel order inside a row has no direct vector form
that the loop vectorizer finds. A hand-written NEON reversal was not tried.

## The clipped blur

`box_blur_clipped` has the vectorizer report remarks on the loops at the
`x` loop and on the inner `sx` loop (width 8, with "cost model indicates that
vectorization is not beneficial" for some instances). I did not map every
remark to the `uint8_t` or `float` instance, so I do not claim which of the
two benefits. What can be said: the inner loop sums only a few values per
row (a window of 3 to 11 pixels for radius 1 to 5), so the vector set-up
cost is large against the work, and the speed-up of 1.3 to 1.8x measured in
#40 is likely to come from removing the per-neighbour border test rather than
from vectorization.

## Summary

| Loop | Vectorized | Reason given |
|------|-----------|--------------|
| interleaved grayscale, `brighten_channel` | no | uncountable loop (`i += 3`) |
| planar grayscale, brightness, `brighten_channel`, conversions | yes, width 16 | |
| `flip_horizontal` | no | none printed |
| `flip_horizontal_blocked` block loop | no | instruction cannot be vectorized |
| `flip_vertical` (`swap_ranges`) | yes, width 16 | |
| `box_blur_clipped` | partly | cost model, see above |
