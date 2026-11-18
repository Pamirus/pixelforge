#include "pixelforge/image_view.hpp"

#include <stdexcept>

namespace pixelforge {

ImageView ImageView::crop(int x, int y, int width, int height) const
{
    if (width <= 0 || height <= 0) {
        throw std::invalid_argument("crop width and height must be positive");
    }
    // Subtracting instead of comparing x + width with width_, which could overflow.
    if (x < 0 || y < 0 || x > width_ - width || y > height_ - height) {
        throw std::out_of_range("crop rectangle does not fit inside the view");
    }
    return ImageView(pixel(x, y), width, height, stride_);
}

} // namespace pixelforge
