# The negative written three ways: what the compiler makes of it

Read on 2026-11-23 with Apple Clang 21 (arm64), Release flags (`-O3 -DNDEBUG`),
8-bit pixels. Only Clang was looked at; GCC was not run for this note.

The three variants are in
[negative_variants.hpp](../include/pixelforge/negative_variants.hpp). To give
each one its own symbol, [docs/assembly/negative_instances.cpp](assembly/negative_instances.cpp)
wraps them in plain functions (it is not part of the build):

    clang++ -std=c++20 -O3 -DNDEBUG -Iinclude -c docs/assembly/negative_instances.cpp -o neg.o \
        -Rpass=loop-vectorize
    objdump -d --no-show-raw-insn neg.o | c++filt

## What I found

| | raw loop | `std::transform` + lambda | `std::ranges::copy` of a `views::transform` |
|---|---|---|---|
| Instructions in the function | 47 | 47 | 46 |
| Calls | none | none | none |
| Main loop | 64 bytes per turn | 64 bytes per turn | 64 bytes per turn |
| Loop vectorizer | width 16, interleave 4 | width 16, interleave 4 | width 16, interleave 4 |

In all three the main loop is the same six steps: two `ldp q` loads (64
bytes), four `mvn.16b` (bitwise not, which is `255 - x` for 8 bits), two `stp q`
stores and a counter. After it come an 8-byte loop (`ldr d`, `mvn.8b`,
`str d`) and a one-byte loop for the last few values. The lambda, the view and
the iterators are gone: the compiler inlined them, so nothing is called and
nothing is built at run time.

I compared the listings by eye and by instruction counts. The differences are
only around the loops: which registers are used, and how the one-byte tail
ends (the raw loop counts a negative offset up to zero, the other two compare
the pointer with the end). They are not different algorithms. I did not
count cycles, and I do not claim the tails differ in speed; they run for at
most 7 values per row of work here.

## What it says about the benchmark

The run on 2026-11-22 (#43) put the three within the noise of each other, on
a busy machine (0.6 to 0.7 ms, with single runs from 0.57 to 1.06 ms). The
assembly explains why: the hot loop is the same machine code. So the "zero
cost" of the abstraction is real *here*, and it is not a measurement
accident that the gaps are small.

## Limits of this result

- It holds for this compiler, this flag set and this filter. The loop
  vectorizer found the loop through the lambda and through the view. Filters
  that read neighbours (blur, sharpen) or write somewhere else than they read
  were not tried.
- The ranges version writes back into the same place it reads, through
  `std::ranges::copy`. A view that builds a new container would have a cost
  the other two do not (an allocation); that is not what was measured.
- Without `-O3` (a Debug build) none of this holds: the lambda and the
  iterators are real calls there. That was not measured either.
