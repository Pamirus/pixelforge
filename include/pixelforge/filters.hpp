#pragma once

#include "pixelforge/image.hpp"

#include <string>

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

    // Changes the image in place.
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

} // namespace pixelforge
