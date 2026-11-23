#pragma once

// Filters that split the image across threads (stage v10).
//
// The task said OpenMP (`#pragma omp parallel for`). Apple Clang, which this
// project is developed with, ships without OpenMP support, and libomp is not
// installed here, so the loops are split by hand with std::thread. It is the
// same idea: cut the work into as many equal pieces as there are threads, give
// each thread one piece, wait for all of them. It works on every compiler of
// the CI too.
//
// What can be split: the pixelwise filters (every value or pixel depends only
// on itself) and the blur (each output row reads the unchanged source and
// writes only its own row). The pieces never write the same memory, so no
// locking is needed. Each function gives the same pixels as its serial
// version; the tests check that.

#include "pixelforge/image.hpp"

#include <cstddef>
#include <thread>
#include <vector>

namespace pixelforge {

// Calls work(t, begin, end) for t = 0, 1, ... on pieces that together cover
// [0, count), each on its own thread; t numbers the pieces, so a piece can use
// the private storage that belongs to its number. The calling thread does the
// last piece itself. With threads <= 1, or less work than threads, fewer
// threads are started (t is always below max(threads, 1)).
template <typename Work>
void parallel_chunks_indexed(std::size_t count, unsigned threads, Work work)
{
    if (threads > count) {
        threads = static_cast<unsigned>(count);
    }
    if (threads <= 1) {
        work(0u, std::size_t{0}, count);
        return;
    }
    const std::size_t piece = count / threads;
    std::vector<std::thread> started;
    started.reserve(threads - 1);
    for (unsigned t = 0; t + 1 < threads; ++t) {
        started.emplace_back([=] { work(t, t * piece, (t + 1) * piece); });
    }
    work(threads - 1, (threads - 1) * piece, count); // the last piece takes the remainder
    for (std::thread& thread : started) {
        thread.join();
    }
}

// The same without the number of the piece: work(begin, end).
template <typename Work>
void parallel_chunks(std::size_t count, unsigned threads, Work work)
{
    parallel_chunks_indexed(count, threads, [=](unsigned, std::size_t begin, std::size_t end) {
        work(begin, end);
    });
}

// The same as brightness(image, amount), grayscale(image) and
// box_blur_clipped(image, radius) for 8-bit images, on `threads` threads.
void brightness_parallel(Image& image, int amount, unsigned threads);
void grayscale_parallel(Image& image, unsigned threads);
void box_blur_parallel(Image& image, int radius, unsigned threads);

} // namespace pixelforge
