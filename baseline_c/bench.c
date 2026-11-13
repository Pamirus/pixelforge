/* pixelforge_c_bench: times the C baseline filters on one image.
 *
 *     pixelforge_c_bench images/cat-4k.jpg [RUNS]
 *
 * Before every run the original pixels are copied back, outside the timed
 * part, so each run filters the same data. The median and the fastest run
 * are printed. */
#define _POSIX_C_SOURCE 200809L /* clock_gettime */

#include "pf_filters.h"
#include "pf_image.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef void (*filter_fn)(pf_image* image);

static void grayscale(pf_image* image)
{
    pf_grayscale(image);
}

static void brightness_20(pf_image* image)
{
    pf_brightness(image, 20);
}

static double now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e3 + (double)ts.tv_nsec / 1e6;
}

static int compare_doubles(const void* a, const void* b)
{
    const double x = *(const double*)a;
    const double y = *(const double*)b;
    return (x > y) - (x < y);
}

static void time_filter(const char* name, filter_fn filter, const pf_image* source,
                        pf_image* work, double* times, int runs)
{
    const size_t size = pf_image_size(source);
    for (int r = 0; r < runs; ++r) {
        memcpy(work->pixels, source->pixels, size);
        const double start = now_ms();
        filter(work);
        times[r] = now_ms() - start;
    }

    qsort(times, (size_t)runs, sizeof *times, compare_doubles);
    const double median =
        runs % 2 ? times[runs / 2] : (times[runs / 2 - 1] + times[runs / 2]) / 2.0;
    /* Image bytes per second; the filter itself reads and writes each byte. */
    const double gb_per_s = (double)size / (median * 1e6);
    printf("%-12s median %8.3f ms   fastest %8.3f ms   %6.1f GB/s\n", name, median, times[0],
           gb_per_s);
}

int main(int argc, char** argv)
{
    if (argc < 2 || argc > 3) {
        fputs("usage: pixelforge_c_bench IMAGE [RUNS]\n", stderr);
        return 2;
    }
    const int runs = argc == 3 ? atoi(argv[2]) : 30;
    if (runs < 1 || runs > 100000) {
        fputs("pixelforge_c_bench: RUNS must be from 1 to 100000\n", stderr);
        return 2;
    }

    int status = 0;
    pf_image* work = NULL;
    double* times = NULL;
    pf_image* source = pf_image_load(argv[1]);
    if (source == NULL) {
        fprintf(stderr, "pixelforge_c_bench: cannot load %s: %s\n", argv[1],
                pf_image_last_error());
        return 1;
    }
    work = pf_image_create(source->width, source->height);
    times = malloc(sizeof *times * (size_t)runs);
    if (work == NULL || times == NULL) {
        fputs("pixelforge_c_bench: out of memory\n", stderr);
        status = 1;
        goto cleanup;
    }

    printf("%s: %d x %d, %.1f MB, %d runs per filter\n", argv[1], source->width,
           source->height, (double)pf_image_size(source) / 1e6, runs);
    time_filter("grayscale", grayscale, source, work, times, runs);
    time_filter("brightness", brightness_20, source, work, times, runs);

cleanup:
    free(times);
    pf_image_destroy(work);
    pf_image_destroy(source);
    return status;
}
