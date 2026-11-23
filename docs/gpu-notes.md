# How the filters would map to a GPU

A short note. **No GPU code was written and nothing here was measured on a
GPU.** What comes from this project's own measurements is marked "measured",
and everything else is reasoning from how GPUs are generally described, marked
"guess" where it matters.

A GPU runs the same small program (a kernel) on thousands of pixels at once,
each one on its own thread. It does well when every pixel's work is independent
and the same, and when neighbouring threads read neighbouring memory. It does
badly when threads must wait for each other, take different branches or need
the results of other threads.

## Pointwise filters: a good fit

`grayscale`, `brightness`, `negative`, `threshold`. Each output value depends
only on the pixel's own input. One thread per pixel, no communication. This is
also what made them easy to split across CPU threads (v10) and to fuse into one
loop (the lazy chain of v9).

- A GPU kernel for these is limited by memory bandwidth, not by arithmetic. On
  the CPU the library's `brightness` did *not* reach that limit (it used about
  a third of the copy speed, measured in
  [memory-vs-compute.md](memory-vs-compute.md)); a GPU would. (Guess for the GPU
  half.)
- Fusing a chain of such filters into one kernel matters more on a GPU than on
  the CPU: every separate kernel reads and writes the whole image from the
  GPU's memory again. The v9 lazy chain was measured on the CPU only, where it
  was no faster than three in-place passes (5.81 against 5.84 ms); on a GPU
  I would expect more, but did not test it.

## Windowed filters: a good fit, with care

`box_blur`, `sharpen`, `sobel`: each output pixel reads a window of its
neighbours. This is a *stencil*, one of the classic GPU patterns.

- Threads of a block read overlapping windows. The usual approach is to load a
  tile of the image, with a border of `radius` pixels, into the fast memory
  shared by the block once, and let the threads read their windows from there.
  (Guess about the speed-up.)
- The blur here is compute-bound on the CPU (measured: 1.6 to 1.9 G neighbours
  a second at every size). A GPU has far more arithmetic units, so this is a
  filter where it should gain most; by how much I do not know.
- A box blur can be done as two passes, one along the rows and one along the
  columns, with cost proportional to `2 * (2r + 1)` per pixel rather than
  `(2r + 1)^2`. This project's blur does the whole window, because it was
  written to measure the cost of the window. For a GPU the two-pass form would be
  the first thing to try.
- The border: `box_blur_clipped` cuts the window to the image once per pixel.
  Threads at the edge would then do less work than the others and finish early
  (divergence). The cost is small if only the border pixels differ. Not measured.

## Rearranging filters: a fit, but only memory speed to gain

`flip_horizontal`, `flip_vertical`, `rotate_90`. No arithmetic: each pixel is
read from one place and written to another. On the CPU the horizontal flip was
limited by its instructions (measured, v8); the rotation probably by its access
order (the explanation of v3, not re-measured). On a GPU the reading and writing would both want neighbouring threads to
touch neighbouring bytes; a rotation breaks that for one of the two sides, and
the usual remedy is to go through a tile in shared memory. (Guess.)

## Histogram: needs a different shape

Counting gray levels is a reduction: thousands of threads add to 256 counters.
This project measured the same problem on the CPU: shared atomic counters were
up to 16 times slower than one thread (v10), and private counters per thread,
merged at the end, were fastest. The same lesson would apply, with the same
remedy in a different form: private counters per block in shared memory, merged
into the global ones at the end. (The remedy is a guess; the CPU result is
measured.)

## Not a fit: the parts around the filters

- **Loading and saving.** Decoding a JPEG and encoding a PNG took 77 and 570 ms
  for the 4K image on the CPU (measured, `--time`), against 3 to 80 ms for the
  filters. The GPU would not help there, and it would be the new bottleneck.
- **Moving the image.** With a separate GPU, the image has to be copied over
  the bus and back; for a single cheap filter like brightness this can cost more
  than the filter itself. (A general property; on a chip where the CPU and GPU
  share memory, as on Apple's, the copy mostly disappears. I have not checked
  this for this machine.)
- **Branchy and sequential code.** The filter factory, the decorator and the
  virtual dispatch of v7 are about *choosing* what to run. They belong on the
  CPU, and would stay there to build and launch the kernels.

## Summary

| Filter | Fit | Why |
|---|---|---|
| grayscale, brightness, negative, threshold | very good | independent per pixel, bandwidth-limited |
| box blur, sharpen, sobel | good | stencil; tile in shared memory; blur is compute-bound on the CPU |
| flips, rotation | fair | no arithmetic; access order matters |
| histogram | fair, needs a rewrite | contention on the counters, as measured on the CPU |
| loading, saving, filter choice | poor | CPU work, and the new bottleneck |
