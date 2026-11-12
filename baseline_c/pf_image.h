/* C baseline: an RGB image held with malloc and free.
 *
 * Every later C++ stage is measured against this code, so it stays plain C:
 * manual allocation, manual cleanup, and error codes instead of exceptions. */
#ifndef PF_IMAGE_H
#define PF_IMAGE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Pixels are stored row by row as R, G, B bytes: RGBRGBRGB... */
#define PF_CHANNELS 3

typedef struct pf_image {
    int width;
    int height;
    unsigned char* pixels; /* width * height * PF_CHANNELS bytes, owned by the image */
} pf_image;

/* Allocates an image with uninitialised pixels.
   Returns NULL if the size is not positive or memory runs out. */
pf_image* pf_image_create(int width, int height);

/* Frees the image and its pixels. Accepts NULL. */
void pf_image_destroy(pf_image* image);

/* Size of the pixel buffer in bytes. */
size_t pf_image_size(const pf_image* image);

/* Loads a PNG, JPEG, BMP or other stb-supported file and converts it to RGB.
   Returns NULL on failure. */
pf_image* pf_image_load(const char* path);

/* Saves the image; the format follows the extension: .png, .jpg/.jpeg or .bmp.
   Returns 0 on success and -1 on failure. */
int pf_image_save(const pf_image* image, const char* path);

/* Describes the last failure of the functions above. The message lives in a
   global, so it is not thread-safe. */
const char* pf_image_last_error(void);

#ifdef __cplusplus
}
#endif

#endif /* PF_IMAGE_H */
