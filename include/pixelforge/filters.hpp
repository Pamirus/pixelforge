#pragma once

#include "pixelforge/image.hpp"
#include "pixelforge/image_view.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>

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

// The 8-bit work of brightness on count bytes: adds amount (clamped to
// -255..255) to each and keeps the result in 0..255. brightness and
// brightness_parallel both use it, so they cannot drift apart.
void brighten_bytes(std::uint8_t* p, std::size_t count, int amount);

// The brightness of 8-bit images as a lookup table: entry v is what
// brightness(image, amount) turns the channel value v into. The function is
// constexpr, so it can build a table while the program is compiled, as the
// static_asserts in the tests do, or while it runs.
[[nodiscard]] constexpr std::array<std::uint8_t, 256> brightness_table(int amount)
{
    amount = std::clamp(amount, -255, 255);
    std::array<std::uint8_t, 256> table{};
    for (int v = 0; v < 256; ++v) {
        const int result = std::clamp(v + amount, 0, 255);
        table[static_cast<std::size_t>(v)] = static_cast<std::uint8_t>(result);
    }
    return table;
}

// The same result as brightness(image, amount), but each channel value is
// looked up in brightness_table(amount) instead of being computed. Building
// the table costs 256 steps, against about 25 million channel values in a 4K
// photo.
void brightness_lookup(Image& image, int amount);

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

// The same result as flip_horizontal, written differently: instead of swapping
// two pixels at a time it takes a block of 16 pixels from each end of the row
// into small buffers and writes them back mirrored. It exists to find out why
// flip_horizontal is slower than flip_vertical (see the v8 results in the
// README); use flip_horizontal in the rest of the program.
template <typename T>
void flip_horizontal_blocked(BasicImageView<T> view);

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

template <typename T>
void flip_horizontal_blocked(BasicImage<T>& image)
{
    flip_horizontal_blocked(BasicImageView<T>(image));
}

// The image turned 90 degrees clockwise. Width and height swap, so the result
// is a new image; the original is not changed.
template <typename T>
[[nodiscard]] BasicImage<T> rotate_90(const BasicImage<T>& image);

// A box blur: every channel of every pixel becomes the average of the same
// channel in the k × k pixels around it, where k = 2 * radius + 1. Radius 1
// averages 3 × 3 pixels, radius 2 averages 5 × 5. Near the border the
// neighbours outside the image are left out, and the average is taken over
// the rest. Radius 0 changes nothing. Throws std::invalid_argument if radius
// is negative.
template <typename T>
void box_blur(BasicImage<T>& image, int radius);

// The same result as box_blur, with the border handled once per pixel instead
// of once per neighbour. box_blur asks for every pixel of the window whether
// it lies inside the image. This version first cuts the window down to the
// part that does, with a few min and max calls, and then runs plain loops over
// that rectangle with no test inside them. The pixels it adds up, and the
// order it adds them in, are the same, so the result is the same, also for
// float images. Stage v8 compares the speed of the two.
template <typename T>
void box_blur_clipped(BasicImage<T>& image, int radius);

// The work of box_blur_clipped for the rows y_begin to y_end - 1 only: reads
// `source` (an unchanged copy of the image) and writes those rows of `image`.
// Two threads that are given different rows write different memory and read
// only `source`, so they can run at the same time (stage v10).
template <typename T>
void box_blur_clipped_rows(const BasicImage<T>& source, BasicImage<T>& image, int radius,
                           int y_begin, int y_end);

// Sharpens the image with the 3 × 3 kernel
//
//      0 -1  0
//     -1  5 -1
//      0 -1  0
//
// applied to each channel: five times the pixel minus its four neighbours,
// clamped to 0..255 (0.0..1.0). A pixel on the border uses the nearest pixel
// of the image for a neighbour that is outside it, so a flat area is not
// changed anywhere.
template <typename T>
void sharpen(BasicImage<T>& image);

// Sobel edge detection. The image is turned into gray levels (the average of R,
// G and B, as in grayscale). Two 3 × 3 kernels measure the change from left to
// right (gx) and from top to bottom (gy) around each pixel, and the pixel
// becomes sqrt(gx² + gy²), clamped to 0..255 (0.0..1.0), in R, G and B: bright
// where the gray level changes fast, black in flat areas. Neighbours outside
// the image are replaced as in sharpen.
template <typename T>
void sobel(BasicImage<T>& image);

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

class BoxBlur : public Filter {
public:
    explicit BoxBlur(int radius) : radius_(radius) {}
    void apply(Image& image) const override;
    std::string description() const override;

private:
    int radius_;
};

class Sharpen : public Filter {
public:
    void apply(Image& image) const override;
    std::string description() const override;
};

class Sobel : public Filter {
public:
    void apply(Image& image) const override;
    std::string description() const override;
};

namespace detail {

// True if a Filter object of type F can be applied to a BasicImage<T>, that
// is, if filter.apply(image) is a valid expression for a const filter. This is
// the pre-C++20 way of asking the question (v6 compares it with a concept):
// when the expression inside decltype is invalid, substitution fails, the
// partial specialization is dropped without an error, and the primary
// template, which says false, is used instead. That is SFINAE.
template <typename F, typename T, typename = void>
struct is_filter_for : std::false_type {};

template <typename F, typename T>
struct is_filter_for<
    F, T, std::void_t<decltype(std::declval<const F&>().apply(std::declval<BasicImage<T>&>()))>>
    : std::true_type {};

template <typename F, typename T>
inline constexpr bool is_filter_for_v = is_filter_for<F, T>::value;

} // namespace detail

// A filter chain: applies the filters to the image in the order given, as in
// apply(image, Grayscale{}, Brightness{20}, Blur<3>{}). It is written twice,
// once with std::enable_if (apply_enable_if) and once with a concept (apply).
// Each filter only needs an apply(image) member, so the filter classes above
// and Blur<Size> from blur.hpp can be mixed. Unlike a list of std::unique_ptr<Filter>, the chain
// is fixed when the program is compiled and the compiler knows the type of
// every filter in it. Blur<Size>::apply is then an ordinary call that can be
// inlined. The filter classes' apply stays virtual, because a class derived
// from Grayscale could override it; stage v7 looks at that with final.
//
// The filters are taken by const reference, so temporaries such as
// Grayscale{} work and nothing is copied. The comma fold expression below
// expands to filter1.apply(image), filter2.apply(image), ..., which runs left
// to right. With no filters it does nothing.
//
// The enable_if in the return type says that every filter must be applicable
// to this image. Without it, a type that is not a filter would be reported
// from inside the fold expression. With it, such a call has no matching
// function: apply_enable_if(float_image, Grayscale{}) is rejected, because the
// filter classes work on 8-bit images only.
//
// This is the version from before C++20, kept next to the concept version
// below so the two can be compared. Use apply in new code.
template <typename T, typename... Filters>
std::enable_if_t<(detail::is_filter_for_v<Filters, T> && ...)>
apply_enable_if(BasicImage<T>& image, const Filters&... filters)
{
    (filters.apply(image), ...);
}

// The same constraint as a C++20 concept: a type F is a filter for images of
// channel type T if filter.apply(image) is valid for a const filter and a
// BasicImage<T>&. It accepts and rejects the same types as
// detail::is_filter_for, but reads as what it says, and the compiler can name
// it in its error messages. (It is not called Filter, which is the abstract
// class above.)
template <typename F, typename T>
concept FilterFor = requires(const F& filter, BasicImage<T>& image) { filter.apply(image); };

// The filter chain, constrained with the concept. FilterFor<T>... means that
// each of the filters satisfies FilterFor<Filter, T>.
template <typename T, FilterFor<T>... Filters>
void apply(BasicImage<T>& image, const Filters&... filters)
{
    (filters.apply(image), ...);
}

} // namespace pixelforge
