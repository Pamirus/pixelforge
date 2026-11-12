#include "pf_filters.h"

void pf_grayscale(pf_image* image)
{
    unsigned char* p = image->pixels;
    const size_t size = pf_image_size(image);
    for (size_t i = 0; i < size; i += PF_CHANNELS) {
        const unsigned sum = (unsigned)p[i] + p[i + 1] + p[i + 2];
        const unsigned char gray = (unsigned char)((sum + 1) / 3); /* rounded */
        p[i] = gray;
        p[i + 1] = gray;
        p[i + 2] = gray;
    }
}

static unsigned char clamp_to_byte(int value)
{
    if (value < 0) {
        return 0;
    }
    if (value > 255) {
        return 255;
    }
    return (unsigned char)value;
}

void pf_brightness(pf_image* image, int delta)
{
    /* Larger steps change nothing more, and limiting delta keeps p[i] + delta
       far away from int overflow. */
    if (delta > 255) {
        delta = 255;
    } else if (delta < -255) {
        delta = -255;
    }

    unsigned char* p = image->pixels;
    const size_t size = pf_image_size(image);
    for (size_t i = 0; i < size; ++i) {
        p[i] = clamp_to_byte(p[i] + delta);
    }
}
