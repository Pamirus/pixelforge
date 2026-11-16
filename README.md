# pixelforge

[![CI](https://github.com/Pamirus/pixelforge/actions/workflows/ci.yml/badge.svg)](https://github.com/Pamirus/pixelforge/actions/workflows/ci.yml)

A small command-line photo filter program, and a lab for measuring what modern C++ techniques really cost.

pixelforge loads a photo, applies filters such as grayscale, brightness, blur or threshold, and saves the result. The program is rewritten in stages. It starts as plain C with `malloc` and `free`, then moves through RAII, move semantics, views, templates, concepts, data layout, parallelism and lazy evaluation. Each stage gets a git tag (`v0`, `v1`, …) and a benchmark, so any two stages can be compared.

**Status:** v0, the C baseline, and v1, an `Image` class built on RAII, are done. Next is v2, copying versus moving images. The plan is in [docs/ROADMAP.md](docs/ROADMAP.md).

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

The first configure downloads GoogleTest. Pass `-DPIXELFORGE_BUILD_TESTS=OFF` to skip the tests.

## Results

Time for one filter pass over the 3840 × 2160 photo (24.9 MB in memory):

```sh
build/baseline_c/pixelforge_c_bench images/cat-4k.jpg 200   # v0
build/benchmarks/pixelforge_bench images/cat-4k.jpg 200      # v1
```

| Tag | Version | Grayscale | Brightness |
|---|---|---|---|
| v0 | C, `malloc`/`free` | 4.1 ms | 2.2 ms |
| v1 | C++ `Image` class: RAII, `std::vector`, exceptions | 4.1 ms | 1.8 ms |

Each benchmark run times 200 passes and reports their median. The two benchmarks were run one after the other, three times each, on 2026-11-16; the table shows the middle of the three medians.

Machine: Apple A18 Pro (6 cores, 8 GB), Apple Clang 21, Release build (`-O3`). On this laptop timings vary from run to run and even more from day to day: on 2026-11-13 the same v0 benchmark gave 6.2 ms and 3.1 ms. So only rows measured in the same session are compared, and the whole table is measured again whenever a row is added.

- **Grayscale** is the same in C and C++. Keeping the pixels in a class with a `std::vector` costs nothing measurable.
- **Brightness** was about 20% faster in C++ in all three rounds. Both versions process 16 bytes at a time, but their inner loops differ. In C++, `std::clamp(v, 0, 255)` becomes one max and one min per vector. In C, the two `if`s in `clamp_to_byte` become a mask and extra shuffle steps: 23 instructions per 16 bytes instead of 19. That is the likely cause, and it comes from the code the compiler chose, not from the language. Stage v9 compares assembly in detail.
- **Grayscale takes about twice as long as brightness**, although both touch the same 24.9 MB. Stage v8 looks into why.

## Third-party code

| Library | Used for | License |
|---|---|---|
| [stb_image, stb_image_write](https://github.com/nothings/stb) | Reading and writing PNG and JPEG, vendored in `third_party/stb` | MIT or public domain |
| [GoogleTest](https://github.com/google/googletest) 1.18.0 | Unit tests, downloaded at configure time | BSD 3-Clause |

## License

MIT. See [LICENSE](LICENSE).
