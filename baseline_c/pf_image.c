#include "pf_image.h"

#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <stb_image.h>
#include <stb_image_write.h>

static const char* last_error = "";

const char* pf_image_last_error(void)
{
    return last_error;
}

pf_image* pf_image_create(int width, int height)
{
    if (width <= 0 || height <= 0) {
        last_error = "image width and height must be positive";
        return NULL;
    }
    if ((size_t)width > SIZE_MAX / PF_CHANNELS / (size_t)height) {
        last_error = "image is too large";
        return NULL;
    }

    pf_image* image = malloc(sizeof *image);
    if (image == NULL) {
        last_error = "out of memory";
        return NULL;
    }
    image->width = width;
    image->height = height;
    image->pixels = malloc((size_t)width * (size_t)height * PF_CHANNELS);
    if (image->pixels == NULL) {
        free(image);
        last_error = "out of memory";
        return NULL;
    }
    return image;
}

void pf_image_destroy(pf_image* image)
{
    if (image == NULL) {
        return;
    }
    free(image->pixels);
    free(image);
}

size_t pf_image_size(const pf_image* image)
{
    return (size_t)image->width * (size_t)image->height * PF_CHANNELS;
}

pf_image* pf_image_load(const char* path)
{
    int width = 0;
    int height = 0;
    int channels_in_file = 0;
    /* stb converts the file to RGB and allocates the buffer with malloc
       (stb_impl.c keeps the default STBI_MALLOC), so the image can own it
       and pf_image_destroy can free it. */
    unsigned char* pixels = stbi_load(path, &width, &height, &channels_in_file, PF_CHANNELS);
    if (pixels == NULL) {
        last_error = stbi_failure_reason();
        return NULL;
    }

    pf_image* image = malloc(sizeof *image);
    if (image == NULL) {
        stbi_image_free(pixels);
        last_error = "out of memory";
        return NULL;
    }
    image->width = width;
    image->height = height;
    image->pixels = pixels;
    return image;
}

/* Case-insensitive check that path ends with extension, for example ".png". */
static int has_extension(const char* path, const char* extension)
{
    const size_t path_length = strlen(path);
    const size_t extension_length = strlen(extension);
    if (path_length < extension_length) {
        return 0;
    }
    const char* tail = path + path_length - extension_length;
    for (size_t i = 0; i < extension_length; ++i) {
        if (tolower((unsigned char)tail[i]) != tolower((unsigned char)extension[i])) {
            return 0;
        }
    }
    return 1;
}

int pf_image_save(const pf_image* image, const char* path)
{
    const int width = image->width;
    const int height = image->height;
    int written = 0;

    if (has_extension(path, ".png")) {
        written = stbi_write_png(path, width, height, PF_CHANNELS, image->pixels,
                                 width * PF_CHANNELS);
    } else if (has_extension(path, ".jpg") || has_extension(path, ".jpeg")) {
        written = stbi_write_jpg(path, width, height, PF_CHANNELS, image->pixels, 90);
    } else if (has_extension(path, ".bmp")) {
        written = stbi_write_bmp(path, width, height, PF_CHANNELS, image->pixels);
    } else {
        last_error = "unsupported output format (use .png, .jpg or .bmp)";
        return -1;
    }

    if (!written) {
        last_error = "could not write the output file";
        return -1;
    }
    return 0;
}
