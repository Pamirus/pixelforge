# pixelforge: what was measured, what turned out wrong, what is open

pixelforge is a small photo filter program rewritten in twelve stages (v0 to
v11, see [ROADMAP.md](ROADMAP.md)) to find out what modern C++ techniques cost
on a real image: 3840 × 2160 pixels, 24.9 MB. Each stage added a technique, a
test and a benchmark; the numbers are in the [README](../README.md) and the
longer notes in this folder. Everything was measured on one machine (Apple A18
Pro, six cores, Apple Clang 21, Release build) and, unless said, on a machine
that was not quiet. This page collects the results and, as important, the
places where the first idea was wrong.

## What the stages showed

| Stage | Question | Answer, with the evidence |
|---|---|---|
| v0 to v1 | Does a C++ class cost anything over C functions? | No: grayscale is the same in both; brightness was 16 to 22% faster in C++, from the code the compiler chose. |
| v2 | Copy or move? | A move costs about 1 ns whatever the size; a copy of 24.9 MB about a million times that. The first copy in a process is the slow one (fresh pages). |
| v3 | Views, flips, rotation | A crop costs nanoseconds; copying it, about 100,000 times more. The horizontal flip is 8 to 15 times slower than the vertical one. |
| v4 | Templates for `uint8_t` and `float` | Identical machine code for 8-bit pixels. `float` brightness takes 40% longer for four times the bytes. |
| v5 | Work at compile time | `Blur<N>` is 1.3 to 2.6 times faster than the run-time blur; a lookup table for brightness is 2.5 times slower. |
| v6 | Concepts against `enable_if` | Same behaviour; Clang's error names the failing filter, g++ 15.2's does not. |
| v7 | Four ways to choose a filter | `virtual`, CRTP, `std::variant`, `std::function` cost about the same; `final` saves about half a nanosecond per call. |
| v8 | Memory layout | Planar storage is 2.4 times faster for grayscale and 4.4 times for one channel. The horizontal flip is limited by instructions, not by access order. Clipping the blur window once per pixel: 1.3 to 1.8 times. |
| v9 | Zero-cost abstraction | Raw loop, `std::transform` and ranges compile to the same loop. A lazy expression-template chain equals three in-place passes and beats a new image per step by 1.7 times. |
| v10 | Threads and patterns | Only the blur scales with threads (2.8 times on six). A racy histogram loses counts; atomic counters are up to 16 times slower than serial; private counters are 2.6 times faster. |
| v11 | Across the tags; memory against arithmetic | Nothing real changed in two filters from v0 to v10. The blur is compute-bound; brightness is limited by its instructions, not by the memory. |

## Guesses that turned out wrong, or weaker than I wrote

These are the most useful part. Each was written down as a likely cause or an
expectation, and a later measurement or a look at the assembly changed it.

1. **"Brightness is memory-bound"** (the roadmap, and the explanation of v10).
   The library's brightness runs at the same 18 GiB/s on a 48 KB image as on a
   50 MB one, and a copy reaches 53 to 126 GiB/s. It is limited by its
   instructions. The same filter in 8-bit arithmetic is 3.6 times faster and
   *is* memory-bound. ([memory-vs-compute.md](memory-vs-compute.md))
2. **"The clamp is one max and one min per vector"** (README, v4 and v5). The
   assembly shows the bytes widened to 32 bits and four max and four min on
   four-lane vectors for every 16 bytes. This is the cause of 1. Corrected in
   the README.
3. **"Cores sharing the memory slow brightness down with threads"** (v10). In
   the light of 1, this guess is weaker too. Starting threads on every call is the
   other guess; neither was tested.
4. **"Grayscale is slow because of the interleaved layout"** (v8 as first
   stated). The compiler's report says the loop is *uncountable*: the counter
   steps by 3 and could wrap. A test function that counts pixels instead is
   vectorized. The layout still matters (planar was 2.4 times faster), but the
   loop shape was the first obstacle. ([vectorization.md](vectorization.md))
5. **"The horizontal flip is 15 times slower, because of the access pattern"**
   (v3). Half right: 8.6 times in a later run, so "roughly 8 to 15"; and the
   access pattern is not the cause, since a plain copy of the same bytes takes
   1 ms. The flip is limited by its byte-wise instructions. A flip written
   with vector instructions was not tried.
6. **"One pass should beat three passes"** (v9). The lazy chain tied with
   three in-place passes (5.81 against 5.84 ms). I do not know why.
7. **The first lazy chain was four times slower than the passes it should
   beat.** A store through `uint8_t*` may change any object, so the compiler
   read the source pointer and the filters again after every store and did not
   vectorize the loop. Copying them into local variables fixed it.
8. **False sharing and the flat image** (v10). The bad counter layout is slow
   on the noisy image, as expected, and *fast* on the flat one, where I
   expected the worst fight over a cache line. Unexplained; I did not invent a
   reason.
9. **The plan said OpenMP.** Apple Clang has none, and libomp is not installed
   here, so the threads are `std::thread`.
10. **The v7 dispatch gaps at 4K (about 10%)** were attributed to the compiler
    seeing the loops or not. That was a guess and the assembly was never
    compared.

## What is open

- **Brightness written in 8-bit arithmetic** (with `uqadd` and `uqsub`, so
  negative amounts work too) would make the library's own filter 3.6 times
  faster on this machine. It is the clearest next change, and it was not made
  because it was outside the stage's question.
- **A grayscale loop that counts pixels** vectorizes in a test function; it was
  not put into the library or timed.
- **A flip with NEON reverse instructions**, a **thread pool** instead of new
  threads per call, a **blur with a running sum** or in two passes: not tried.
- **A quiet machine.** Almost every measurement was taken with `cpptools` or
  `godot` using most of a core. Most conclusions are ratios that I expect to
  survive, but the v7 to v10 rise in the tag history, and every gap of less
  than 10 to 30%, should be repeated on a quiet machine.
- **GCC.** Only Clang's remarks and assembly were read, apart from the v6 error
  messages. The CI builds with GCC and passes the tests; nothing about its code
  was measured.
- **`float` images** were measured for the blur (v4 and v8), but not in the layout,
  flip, memory-against-compute, thread and histogram experiments.
- **The rotation** is still explained by its access order only because v3
  said so; it was not re-measured.
- **The history** has two filters, because only those two are timed by the
  program that exists at every tag.

## How it was checked

196 tests (Google Test, including the command-line tests), run on Ubuntu with GCC and on macOS with Apple
Clang by GitHub Actions at every commit, and locally also under AddressSanitizer
and UndefinedBehaviorSanitizer. The parallel filters and the histograms were
also run under ThreadSanitizer, which reports the deliberately racy histogram
and nothing else. Benchmarks use Google Benchmark with random interleaving, the
middle of three runs, and note the state of the machine.
