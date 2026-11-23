# Roadmap

pixelforge is built in twelve stages. Each stage applies one group of C++ ideas to the same program, ends with a git tag (`v0` … `v11`), and adds a row to the results table in the README. Every stage stays tagged, so any two of them can be checked out and benchmarked against each other.

## How the work is done

- Work happens in short sessions, a few per week. A session finishes one or two items below, makes one commit per item, and pushes at the end.
- Every push is built and tested on Ubuntu and macOS by GitHub Actions.
- `baseline_c/` is the reference point for comparisons and is never deleted.
- Benchmarks always use Release builds.

## Stages

| Tag | Stage | C++ topics | Week of |
|---|---|---|---|
| v0 | C baseline | C roots: `malloc`, `free`, manual cleanup | 2026-11-09 |
| v1 | `Image` class | RAII, Rule of Five → Rule of Zero, exceptions | 2026-11-16 |
| v2 | Copy vs move | Copy and move semantics, RVO/NRVO | 2026-11-23 |
| v3 | Views | Ownership, `std::span`, non-owning views, `unique_ptr` | 2026-11-30 |
| v4 | Templates | Generic programming, one filter for `uint8_t` and `float` | 2026-12-07 |
| v5 | Compile time | `constexpr`, `if constexpr`, variadic templates | 2026-12-14 |
| v6 | Concepts | `enable_if` and SFINAE → `concept` and `requires` | 2026-12-21 |
| v7 | Polymorphism | `virtual`, CRTP, `std::variant`, `std::function`, `final` | 2026-12-28 |
| v8 | Memory layout | Cache lines, AoS vs SoA, branch-free inner loops, SIMD | 2027-01-04 |
| v9 | Zero-cost abstraction | Loops vs STL vs ranges in assembly, expression templates | 2027-01-11 |
| v10 | Parallelism and patterns | OpenMP, data races, atomics, false sharing, Factory, Decorator | 2027-01-18 |
| v11 | Measurement | Benchmarks of all tags, memory- vs compute-bound, GPU notes | 2027-01-25 |

## v0 — C baseline

- [x] README, `.gitignore` and this roadmap
- [x] Vendor `stb_image` and `stb_image_write`
- [x] CMake build: C++20, Release by default
- [x] GoogleTest with a PNG round-trip smoke test
- [x] GitHub Actions: build and test on Ubuntu and macOS
- [x] `baseline_c/`: image struct with `malloc`/`free`, load and save through stb
- [x] Grayscale and brightness filters in C
- [x] `pixelforge_c` command line: `pixelforge_c in.png out.png --grayscale --brightness 20`
- [x] Tests for the C filters: after grayscale R = G = B, brightness saturates at 255
- [x] Sample photo in `images/`, first timing on a 4K image in the README, tag `v0`

## v1 — `Image` class

- [x] `Image` that owns its pixels with `new[]`/`delete[]` and follows the Rule of Five
- [x] The same class reduced to the Rule of Zero with `std::vector`
- [x] `load` and `save` that throw when a file is missing or unreadable
- [x] `ScopedTimer` that reports how long a scope took
- [x] `pixelforge` command line with grayscale, brightness, negative and threshold
- [x] Tests, README row, tag `v1`

## v2 — Copy vs move

- [x] Image variant that counts its copies and moves
- [x] Google Benchmark: copying vs moving a 4K image (about 25 MB)
- [x] Return images from functions and confirm RVO/NRVO with the counters
- [x] README row, tag `v2`

## v3 — Views

- [x] `ImageView`: crop a region without copying pixels
- [x] Rows exposed as `std::span`
- [x] Filter objects held in `std::unique_ptr`
- [x] Horizontal flip, vertical flip, 90° rotation
- [x] README row, tag `v3`

## v4 — Templates

- [x] `Image<T>` for `std::uint8_t` and `float` pixels
- [x] Filters written once for both pixel types
- [x] Box blur: the average of a k×k neighbourhood
- [x] README row, tag `v4`

## v5 — Compile time

- [x] Kernel size as a template parameter: `Blur<5>`
- [x] Brightness lookup table built with `constexpr`
- [x] `if constexpr` branches for integer and float pixels
- [x] Filter chains: `apply(img, Grayscale{}, Blur<3>{})`
- [x] README row, tag `v5`

## v6 — Concepts

- [x] Constrain `apply` with `std::enable_if`
- [x] The same constraint as `concept Filter`
- [x] Compare the compiler error messages of both in `docs/`
- [x] Tag `v6`

## v7 — Polymorphism

- [x] One filter written with `virtual`, CRTP, `std::variant` and `std::function`
- [x] Benchmark the four and check devirtualization with `final`
- [x] Sharpen and Sobel edge filters (optional)
- [x] README row, tag `v7`

## v8 — Memory layout

- [x] Interleaved (`RGBRGB…`) vs planar (`RRR…GGG…BBB…`) storage, benchmarked
- [x] Horizontal vs vertical flip timing
- [x] Blur with the border checks moved out of the inner loop
- [x] Read the compiler's vectorization report
- [x] README row, tag `v8`

## v9 — Zero-cost abstraction

- [x] One filter as a raw loop, as an STL algorithm with a lambda, and with ranges
- [x] Compare their assembly in `docs/`
- [x] Lazy filter chains with expression templates, without intermediate images
- [x] README row, tag `v9`

## v10 — Parallelism and design patterns

- [x] Split the image across cores with OpenMP
- [x] Histogram: data race → atomics → per-thread bins and false sharing
- [x] Factory that builds filters from command-line names such as `blur`
- [x] Decorator that times each filter
- [x] README row, tag `v10`

## v11 — Measurement

- [x] One script that benchmarks every tag and plots the results
- [x] Why brightness is memory-bound and a large blur is compute-bound
- [ ] Short note on how the filters would map to a GPU
- [ ] Final write-up in `docs/`, tag `v11`
