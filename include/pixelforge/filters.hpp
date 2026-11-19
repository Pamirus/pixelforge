#pragma once

#include "pixelforge/image.hpp"
#include "pixelforge/image_view.hpp"

#include <string>

namespace pixelforge {

// The filters are function templates, written once for Image and FloatImage.
// They change the image in place, except for the rotation, which returns a
// new image. On 8-bit images, grayscale and brightness use the same arithmetic
// as the C baseline, so their output is identical and their speed can be
// compared directly.
//
// Settings such as the brightness amount and the threshold level are on the
// 0 to 255 scale for both image types, so a setting means the same on either.
// For a FloatImage they are divided by 255.
//
// The templates are defined in filters.cpp, which instantiates them for the
// two channel types. Callers need only these declarations, and the filter
// code is compiled once instead of in every file that uses it.

// Sets R, G and B of every pixel to their average. On 8-bit images the
// average is rounded to the nearest whole number.
template <typename T>
void grayscale(BasicImage<T>& image);

// Adds amount to every channel. Results are clamped to 0..255 (0.0..1.0).
template <typename T>
void brightness(BasicImage<T>& image, int amount);

// Replaces every channel value v with 255 - v (1.0 - v).
template <typename T>
void negative(BasicImage<T>& image);

// Makes a pixel white if its gray level (the average of R, G and B, as in
// grayscale) is at least level, and black otherwise.
template <typename T>
void threshold(BasicImage<T>& image, int level);

// Mirrors the pixels left to right. The flips take a view, so they work on a
// whole image or on a crop of one.
template <typename T>
void flip_horizontal(BasicImageView<T> view);

// Mirrors the rows top to bottom.
template <typename T>
void flip_vertical(BasicImageView<T> view);

// Template argument deduction does not look at conversions, so for an image
// the view overloads above would not be found: T cannot be deduced from
// BasicImageView<T> when the argument is a BasicImage<T>. These overloads make
// the view themselves.
template <typename T>
void flip_horizontal(BasicImage<T>& image)
{
    flip_horizontal(BasicImageView<T>(image));
}

template <typename T>
void flip_vertical(BasicImage<T>& image)
{
    flip_vertical(BasicImageView<T>(image));
}

// The image turned 90 degrees clockwise. Width and height swap, so the result
// is a new image; the original is not changed.
template <typename T>
[[nodiscard]] BasicImage<T> rotate_90(const BasicImage<T>& image);

// The same filters as objects with a common interface, so that a list of
// different filters can be built at run time and applied in order. Each
// object keeps its settings and calls the function above.
class Filter {
public:
    Filter() = default;

    // Copying through a Filter reference would copy only the Filter part of
    // the object (slicing). Filters are owned through pointers instead, so
    // they are never copied.
    Filter(const Filter&) = delete;
    Filter& operator=(const Filter&) = delete;

    // Virtual, because a std::unique_ptr<Filter> deletes the object through a
    // pointer to Filter. Otherwise only ~Filter would run, which is undefined
    // behavior.
    virtual ~Filter() = default;

    // Changes the image. Rotate90 replaces it with a new one of another shape.
    virtual void apply(Image& image) const = 0;

    // The filter and its setting, such as "brightness 20".
    virtual std::string description() const = 0;
};

class Grayscale : public Filter {
public:
    void apply(Image& image) const override;
    std::string description() const override;
};

class Brightness : public Filter {
public:
    explicit Brightness(int amount) : amount_(amount) {}
    void apply(Image& image) const override;
    std::string description() const override;

private:
    int amount_;
};

class Negative : public Filter {
public:
    void apply(Image& image) const override;
    std::string description() const override;
};

class Threshold : public Filter {
public:
    explicit Threshold(int level) : level_(level) {}
    void apply(Image& image) const override;
    std::string description() const override;

private:
    int level_;
};

class FlipHorizontal : public Filter {
public:
    void apply(Image& image) const override;
    std::string description() const override;
};

class FlipVertical : public Filter {
public:
    void apply(Image& image) const override;
    std::string description() const override;
};

class Rotate90 : public Filter {
public:
    void apply(Image& image) const override;
    std::string description() const override;
};

} // namespace pixelforge
