# Memory-bound or compute-bound?

Measured on 2026-11-23 (13:34 to 13:40) with `benchmarks/bound.cpp`, Apple A18 Pro,
Apple Clang 21, Release. The machine was quiet before the runs (`wait_idle` saw
quiet after 99 checks); one process, `godot`, used 99% of a core in the snapshot
taken right after them, so some of the runs may have shared the chip with it.
Each number is the middle of three runs of five repetitions, with the
repetitions in random order.

## The question and how it is tested

A loop is *memory-bound* if the speed of the memory limits it, and
*compute-bound* if the number of instructions does. Both look the same on a
4K image, so the test is to change the size of the image while the work per
byte stays the same:

- A memory-bound loop gets slower per byte when the image no longer fits in the
  cache (the A18 Pro has 4 MB of L2 per core group; the 48 KB image lives in the
  fastest cache, the 50 MB one in no cache), and then settles near the speed of
  a plain copy.
- A compute-bound loop takes the same time per byte at every size.

The images are 1024 pixels wide and 16 to 16384 rows (48 KB to 50 MB).

## What was measured

Speed in GiB/s; bytes moved are read plus written for the copy and for
brightness, read only for `read`. Blur is in billions of neighbour pixels read per second.

| Rows (size) | 16 (48 KB) | 128 (393 KB) | 1024 (3 MB) | 4096 (12.6 MB) | 16384 (50 MB) |
|---|---|---|---|---|---|
| `memcpy` | 126 | 102 | 101 | 73 | 53 |
| `brightness` (the library's) | 18.5 | 18.8 | 18.8 | 17.8 | 17.5 |
| `brightness` written in 8 bits | 132 | 91 | 90 | 79 | 65 |
| Add up every byte (`read`) | 13.5 | 12.8 | 12.8 | 12.9 | 13.5 |
| Blur, radius 2 (G neighbours/s) | 1.86 | 1.65 | 1.59 | 1.72 | 1.58 |

At 50 MB the library's brightness takes 5.4 ms, the 8-bit version 1.4 ms and a copy
of the same size 1.8 ms.

## What it shows

**The blur is compute-bound.** It reads about 1.6 to 1.9 billion neighbour
pixels a second, in a cache or out of it: 15% lower at 50 MB than at 48 KB, while the
copy loses 58%. That is 0.5 to 0.6 ns per neighbour, which is three channel
additions and some loop control. The time does not depend on where the data is
(measured). That the cost is the instructions, not the memory, follows from this;
that the inner loop is only partly vectorized (width 8, with the "cost model
indicates that vectorization is not beneficial" remarks in
[vectorization.md](vectorization.md)) is a likely reason why it is not faster.

**The library's brightness is not memory-bound, and the roadmap's guess was
wrong for it.** It runs at 17.5 to 18.8 GiB/s whatever the size: the same in the
fastest cache as out of every cache. A copy goes at 53 GiB/s on the 50 MB image
and at up to 126 in a cache, so the memory could feed this loop three times
faster than it eats. A loop that waits for memory speeds up when its data is in
the cache; this one does not.

**The assembly says why (likely, read, not tested by changing it).** The hot loop
of `brightness<unsigned char>` widens the bytes to 32-bit numbers (`ushll`,
`uaddw`), clamps them with `smax.4s` and `smin.4s`, and narrows them again, so one
instruction handles 4 values. The clamping is written in `int` because
`std::clamp<int>(p[i] + step, 0, 255)` is. The 8-bit version, `p + 20`, then 255 if the sum
wrapped around, compiles to `uqadd.16b`: 16 values in one instruction. It is
3.6 times faster at 50 MB (1.4 against 5.4 ms) and 7 times in cache, and it *is*
memory-bound: its speed falls from 132 to 65 GiB/s as the image leaves the
cache, and ends near the copy (65 against 53 GiB/s; I did not look into why it
is a little above).

So: **brightness as the filter is written is limited by the instructions it
uses, and could be written to be limited by the memory.** Which of the two a filter
is depends on how it is written as much as on what it does. I did not change
the library's `brightness`; this would be a separate change, and it would need a
negative amount handled too (`uqsub`).

**`read` is a mistake in my benchmark, not a result.** Adding bytes into a 64-bit
sum one by one stays at 13 GiB/s at every size: it is compute-bound for the same
reason, and I did not use it for any conclusion. It is in the table so that it is
not mistaken for a measure of memory speed.

## What this changes in earlier text

The v10 README explains why brightness and grayscale got slower with more
threads with "cores sharing the memory". With this result that guess is weaker:
the library's brightness uses only about a third of what the memory can give, so
sharing the memory is unlikely to be what slows it down with threads. The cost of
starting threads on every call remains the guess to test. Neither was tested.

## What was not done

- Only 8-bit brightness and one blur radius at all sizes (radius 5 was only
  timed on the 4K image in v8). Float images were not run.
- The L2 size above is from the benchmark's own report (4 MB x 6), not from a
  test of mine. Where the speed falls between 3 and 12.6 MB fits it, nothing more.
- No hardware counters (cache misses) were read; the conclusions come from timing
  alone, plus the assembly for brightness.
