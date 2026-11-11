# pixelforge

A small command-line photo filter program, and a lab for measuring what modern C++ techniques really cost.

pixelforge loads a photo, applies filters such as grayscale, brightness, blur or threshold, and saves the result. The program is rewritten in stages. It starts as plain C with `malloc` and `free`, then moves through RAII, move semantics, views, templates, concepts, data layout, parallelism and lazy evaluation. Each stage gets a git tag (`v0`, `v1`, …) and a benchmark, so any two stages can be compared.

**Status:** just started. The plan is in [docs/ROADMAP.md](docs/ROADMAP.md).

## Planned usage

```sh
pixelforge cat.png out.png --grayscale --brightness 20 --blur 5
```
