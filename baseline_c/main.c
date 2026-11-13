/* pixelforge_c: command line of the C baseline.
 *
 *     pixelforge_c in.png out.png --grayscale --brightness 20
 *
 * Filters run in the order they are given. */
#include "pf_filters.h"
#include "pf_image.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum filter_kind { FILTER_GRAYSCALE, FILTER_BRIGHTNESS } filter_kind;

typedef struct filter_step {
    filter_kind kind;
    int amount; /* only used by FILTER_BRIGHTNESS */
} filter_step;

static void print_usage(FILE* out)
{
    fputs("usage: pixelforge_c INPUT OUTPUT [FILTER]...\n"
          "\n"
          "Filters, applied in the order given:\n"
          "  --grayscale       replace R, G and B with their average\n"
          "  --brightness N    add N (-255..255) to every channel\n"
          "\n"
          "OUTPUT must end in .png, .jpg, .jpeg or .bmp.\n",
          out);
}

/* Parses a whole decimal number in [min, max]. Returns 0 on success. */
static int parse_int(const char* text, int min, int max, int* value)
{
    char* end = NULL;
    errno = 0;
    const long parsed = strtol(text, &end, 10);
    if (end == text || *end != '\0' || errno == ERANGE || parsed < min || parsed > max) {
        return -1;
    }
    *value = (int)parsed;
    return 0;
}

int main(int argc, char** argv)
{
    if (argc == 2 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        print_usage(stdout);
        return 0;
    }
    if (argc < 3) {
        print_usage(stderr);
        return 2;
    }

    const char* input = argv[1];
    const char* output = argv[2];
    int status = 0;
    pf_image* image = NULL;

    /* Check every argument before loading anything, so a typo fails fast. */
    filter_step* steps = malloc(sizeof *steps * (size_t)argc);
    if (steps == NULL) {
        fputs("pixelforge_c: out of memory\n", stderr);
        return 1;
    }
    int step_count = 0;
    for (int i = 3; i < argc; ++i) {
        if (strcmp(argv[i], "--grayscale") == 0) {
            steps[step_count++] = (filter_step){FILTER_GRAYSCALE, 0};
        } else if (strcmp(argv[i], "--brightness") == 0) {
            int amount = 0;
            if (i + 1 >= argc || parse_int(argv[i + 1], -255, 255, &amount) != 0) {
                fputs("pixelforge_c: --brightness needs a whole number from -255 to 255\n",
                      stderr);
                status = 2;
                goto cleanup;
            }
            steps[step_count++] = (filter_step){FILTER_BRIGHTNESS, amount};
            ++i;
        } else {
            fprintf(stderr, "pixelforge_c: unknown option '%s'\n\n", argv[i]);
            print_usage(stderr);
            status = 2;
            goto cleanup;
        }
    }

    image = pf_image_load(input);
    if (image == NULL) {
        fprintf(stderr, "pixelforge_c: cannot load %s: %s\n", input, pf_image_last_error());
        status = 1;
        goto cleanup;
    }

    for (int i = 0; i < step_count; ++i) {
        switch (steps[i].kind) {
        case FILTER_GRAYSCALE:
            pf_grayscale(image);
            break;
        case FILTER_BRIGHTNESS:
            pf_brightness(image, steps[i].amount);
            break;
        }
    }

    if (pf_image_save(image, output) != 0) {
        fprintf(stderr, "pixelforge_c: cannot save %s: %s\n", output, pf_image_last_error());
        status = 1;
        goto cleanup;
    }

cleanup:
    /* One exit path that frees everything: the usual C answer to what RAII
       does automatically in C++. */
    pf_image_destroy(image);
    free(steps);
    return status;
}
