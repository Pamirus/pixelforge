#pragma once

#include "pixelforge/image.hpp"

namespace pixelforge {

// The filters change the image in place. Grayscale and brightness use the
// same arithmetic as the C baseline, so their output is identical and their
// speed can be compared directly.

// Sets R, G and B of every pixel to their average, rounded to the nearest
// whole number.
void grayscale(Image& image);

// Adds amount to every channel. Results are clamped to 0..255.
void brightness(Image& image, int amount);

// Replaces every channel value v with 255 - v.
void negative(Image& image);

// Makes a pixel white if its gray level (the rounded average of R, G and B) is
// at least level, and black otherwise.
void threshold(Image& image, int level);

} // namespace pixelforge
