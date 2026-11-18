# pixelforge

[![CI](https://github.com/Pamirus/pixelforge/actions/workflows/ci.yml/badge.svg)](https://github.com/Pamirus/pixelforge/actions/workflows/ci.yml)

A small command-line photo filter program, and a lab for measuring what modern C++ techniques really cost.

pixelforge loads a photo, applies filters such as grayscale, brightness, blur or threshold, and saves the result. The program is rewritten in stages. It starts as plain C with `malloc` and `free`, then moves through RAII, move semantics, views, templates, concepts, data layout, parallelism and lazy evaluation. Each stage gets a git tag (`v0`, `v1`, …) and a benchmark, so any two stages can be compared.

**Status:** v0 (the C baseline), v1 (an `Image` class built on RAII) and v2 (copying versus moving) are done. Next is v3: views that look at part of an image without copying it. The plan is in [docs/ROADMAP.md](docs/ROADMAP.md).

## Usage

Filters run in the order given:

```sh
build/src/pixelforge images/cat-4k.jpg out.jpg --grayscale --brightness 20
```

| Option | Effect |
|---|---|
| `--grayscale` | Replaces R, G and B with their average |
| `--brightness N` | Adds N (−255 to 255) to every channel |
| `--negative` | Replaces every channel value v with 255 − v |
| `--threshold N` | Makes pixels with a gray level of at least N (0 to 255) white, the rest black |
| `--time` | Prints how long loading, each filter and saving take |

The output format follows the extension: `.png`, `.jpg` or `.bmp`. The C baseline, `build/baseline_c/pixelforge_c`, takes the same arguments but has only grayscale and brightness.

| Original | `--grayscale` | `--brightness 60` |
|---|---|---|
| ![Original photo](docs/images/cat.jpg) | ![Grayscale](docs/images/cat-grayscale.jpg) | ![Brightness +60](docs/images/cat-brightness-60.jpg) |

| `--negative` | `--threshold 110` |
|---|---|
| ![Negative](docs/images/cat-negative.jpg) | ![Threshold at 110](docs/images/cat-threshold-110.jpg) |

Photo: "Young tabby cat keeping watch" by W.carter, CC0 ([details](images/README.md)).

## Building

Needs CMake 3.25 or newer and a C++20 compiler.

```sh
cmake -B build        # Release unless CMAKE_BUILD_TYPE says otherwise
cmake --build build
ctest --test-dir build --output-on-failure
```

The first configure downloads GoogleTest and Google Benchmark. Pass `-DPIXELFORGE_BUILD_TESTS=OFF` or `-DPIXELFORGE_BUILD_BENCHMARKS=OFF` to skip them.

## Results

Machine: Apple A18 Pro (6 cores, 8 GB), Apple Clang 21, Release build (`-O3`). On this laptop timings vary from run to run and even more from day to day: on 2026-11-13 the v0 benchmark gave 6.2 ms and 3.1 ms. So only rows measured in the same session are compared, and a whole table is measured again whenever a row is added.

### Filters

Time for one filter pass over the 3840 × 2160 photo (24.9 MB in memory):

```sh
build/baseline_c/pixelforge_c_bench images/cat-4k.jpg 200   # v0
build/benchmarks/pixelforge_bench images/cat-4k.jpg 200      # v1
```

| Tag | Version | Grayscale | Brightness |
|---|---|---|---|
| v0 | C, `malloc`/`free` | 4.1 ms | 2.2 ms |
| v1 | C++ `Image` class: RAII, `std::vector`, exceptions | 4.1 ms | 1.8 ms |

Each benchmark run times 200 passes and reports their median. The two benchmarks were run one after the other, three times each, on 2026-11-16; the table shows the middle of the three medians. v2 did not change the filters, so it has no row here.

- **Grayscale** is the same in C and C++. Keeping the pixels in a class with a `std::vector` costs nothing measurable.
- **Brightness** was about 20% faster in C++ in all three rounds. Both versions process 16 bytes at a time, but their inner loops differ. In C++, `std::clamp(v, 0, 255)` becomes one max and one min per vector. In C, the two `if`s in `clamp_to_byte` become a mask and extra shuffle steps: 23 instructions per 16 bytes instead of 19. That is the likely cause, and it comes from the code the compiler chose, not from the language. Stage v9 compares assembly in detail.
- **Grayscale takes about twice as long as brightness**, although both touch the same 24.9 MB. Stage v8 looks into why.

### Copying versus moving (v2)

```sh
build/benchmarks/pixelforge_benchmarks --benchmark_repetitions=5
```

| Operation on a 3840 × 2160 image (24.9 MB) | Time |
|---|---|
| Copy into a new image | 0.9 ms |
| Copy into an existing image of the same size | 1.0 ms |
| Copy into a new image, first time in a fresh process | 5.5 ms |
| Move to another image and back (two moves) | 1.5 ns |

Measured on 2026-11-18. The first-copy row is the median of five separate processes, which ranged from 3.8 to 9.2 ms; each other row is the middle of three runs of five repetitions. Between repetitions the times varied by 10 to 20%, so 0.9 and 1.0 ms count as the same.

- **A move costs about a nanosecond, whatever the image size.** It hands over a pointer and two sizes; the 24.9 MB stay where they are. A copy reads and writes all of them, which takes about a million times longer.
- **A repeated copy into a new image costs no more than one into an existing image**, although it allocates 24.9 MB each time. macOS's allocator keeps the large block the previous copy freed and hands it out again with its memory pages already in place. With that cache turned off (`MallocLargeCache=0`), the same copy took 2.75 ms.
- **The first copy in a process is the slow one.** Its memory is new, and the system sets up each 16 KB page the first time it is written: about 1,500 pages for one image.
- **Which returns copy and which move** is checked in [tests/return_value_test.cpp](tests/return_value_test.cpp). Returning a temporary or a single named image costs nothing; `return std::move(image)` costs a move; `return first ? a : b` costs a copy.

## Third-party code

| Library | Used for | License |
|---|---|---|
| [stb_image, stb_image_write](https://github.com/nothings/stb) | Reading and writing PNG and JPEG, vendored in `third_party/stb` | MIT or public domain |
| [GoogleTest](https://github.com/google/googletest) 1.18.0 | Unit tests, downloaded at configure time | BSD 3-Clause |
| [Google Benchmark](https://github.com/google/benchmark) 1.10.0 | Microbenchmarks, downloaded at configure time | Apache 2.0 |

## License

MIT. See [LICENSE](LICENSE).
