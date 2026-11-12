/* C baseline filters. They change the image in place. */
#ifndef PF_FILTERS_H
#define PF_FILTERS_H

#include "pf_image.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Replaces R, G and B of every pixel with their rounded average. */
void pf_grayscale(pf_image* image);

/* Adds delta to every channel and clamps the result to 0..255.
   delta is limited to -255..255 first, which already covers every effect. */
void pf_brightness(pf_image* image, int delta);

#ifdef __cplusplus
}
#endif

#endif /* PF_FILTERS_H */
