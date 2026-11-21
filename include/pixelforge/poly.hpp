#pragma once

// Four ways to choose which filter runs, for the same three filters (brightness,
// negative and grayscale): a virtual function, CRTP, a std::variant and a
// std::function. They give the same pixels and differ in how the call finds
// the code to run, which stage v7 measures. The filters are written in this
// header, not in filters.cpp, so that the compiler can see through the calls
// and inline them where it can.
//
// The arithmetic of the three filters is in the small function objects
// BrightnessOp, NegativeOp and GrayscaleOp, shared by all four ways, so a
// difference between the ways cannot come from different loops. On 8-bit
// images they give what brightness, negative and grayscale in filters.hpp give.
//
// The filter classes of filters.hpp (v3) already are the virtual way; the
// classes here repeat it, around the same shared loops, to make it comparable.

#include "pixelforge/image.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <variant>
#include <vector>

namespace pixelforge::poly {

// The shared loops. Each is a function object: a struct with an operator().

struct BrightnessOp {
    int amount;

    void operator()(Image& image) const
    {
        const int step = std::clamp(amount, -255, 255);
        std::uint8_t* p = image.data();
        for (std::size_t i = 0, n = image.size(); i < n; ++i) {
            p[i] = static_cast<std::uint8_t>(std::clamp(p[i] + step, 0, 255));
        }
    }
};

struct NegativeOp {
    void operator()(Image& image) const
    {
        std::uint8_t* p = image.data();
        for (std::size_t i = 0, n = image.size(); i < n; ++i) {
            p[i] = static_cast<std::uint8_t>(255 - p[i]);
        }
    }
};

struct GrayscaleOp {
    void operator()(Image& image) const
    {
        std::uint8_t* p = image.data();
        for (std::size_t i = 0, n = image.size(); i < n; i += Image::channels) {
            const unsigned sum = unsigned{p[i]} + p[i + 1] + p[i + 2];
            const auto gray = static_cast<std::uint8_t>((sum + 1) / 3);
            p[i] = gray;
            p[i + 1] = gray;
            p[i + 2] = gray;
        }
    }
};

// 1. virtual: a base class with a virtual apply, and one class per filter. The
// call looks the function up in the object's vtable at run time, so a list of
// different filters can be built while the program runs, and a new filter
// needs no change to the code that applies them. Every object lives on the
// heap behind a pointer, and the call cannot be inlined unless the compiler
// knows the exact class.

class VirtualFilter {
public:
    VirtualFilter() = default;
    VirtualFilter(const VirtualFilter&) = delete;
    VirtualFilter& operator=(const VirtualFilter&) = delete;
    virtual ~VirtualFilter() = default;

    virtual void apply(Image& image) const = 0;
};

class VirtualBrightness : public VirtualFilter {
public:
    explicit VirtualBrightness(int amount) : op_{amount} {}
    void apply(Image& image) const override { op_(image); }

private:
    BrightnessOp op_;
};

class VirtualNegative : public VirtualFilter {
public:
    void apply(Image& image) const override { op_(image); }

private:
    NegativeOp op_;
};

class VirtualGrayscale : public VirtualFilter {
public:
    void apply(Image& image) const override { op_(image); }

private:
    GrayscaleOp op_;
};

using VirtualChain = std::vector<std::unique_ptr<VirtualFilter>>;

inline void apply_virtual(const VirtualChain& chain, Image& image)
{
    for (const auto& filter : chain) {
        filter->apply(image);
    }
}

// 2. CRTP, the "curiously recurring template pattern": each filter derives
// from CrtpFilter<itself>. The base class knows the derived class as a
// template argument, so it can call into it with a static_cast, and the call
// is resolved when the program is compiled: no vtable, no pointer, and the
// compiler can inline it. The price is that CrtpBrightness and CrtpNegative
// have no common base class, since CrtpFilter<CrtpBrightness> and
// CrtpFilter<CrtpNegative> are different types. A list of them cannot be
// built at run time, only a chain written out in the code, as in v5.

template <typename Derived>
class CrtpFilter {
public:
    void apply(Image& image) const { static_cast<const Derived&>(*this).op_(image); }

private:
    // Only Derived may make a CrtpFilter<Derived>. This turns the mistake
    // "class A : CrtpFilter<B>" into a compile error.
    CrtpFilter() = default;
    friend Derived;
};

class CrtpBrightness : public CrtpFilter<CrtpBrightness> {
public:
    explicit CrtpBrightness(int amount) : op_{amount} {}

private:
    friend class CrtpFilter<CrtpBrightness>;
    BrightnessOp op_;
};

class CrtpNegative : public CrtpFilter<CrtpNegative> {
private:
    friend class CrtpFilter<CrtpNegative>;
    NegativeOp op_;
};

class CrtpGrayscale : public CrtpFilter<CrtpGrayscale> {
private:
    friend class CrtpFilter<CrtpGrayscale>;
    GrayscaleOp op_;
};

// Takes the filters as references to their CrtpFilter bases and expands to
// one static call for each, in the order given.
template <typename... Derived>
void apply_crtp(Image& image, const CrtpFilter<Derived>&... filters)
{
    (filters.apply(image), ...);
}

// 3. std::variant: a value that holds one of a fixed list of types. The
// filters are plain structs with an operator(), no base class and no virtual
// functions. std::visit looks at which type the variant holds and calls the
// matching operator(), usually through a small jump table. A list of
// variants can be built at run time and is stored in one block of memory
// without pointers, but the list of possible filters is fixed in the code
// (here, the three below); adding a filter means changing VariantFilter.

using VariantFilter = std::variant<BrightnessOp, NegativeOp, GrayscaleOp>;

inline void apply_variant(const std::vector<VariantFilter>& chain, Image& image)
{
    for (const VariantFilter& filter : chain) {
        std::visit([&image](const auto& op) { op(image); }, filter);
    }
}

// 4. std::function: holds any callable that can be called with an Image&.
// The filters need no common base class and no common list of types, even a
// lambda is accepted, so it is the most open of the four. It hides the
// callable behind a pointer to a function of its own, much like a virtual
// call, and may allocate memory for a callable that does not fit in its small
// buffer. Calling an empty std::function throws std::bad_function_call.

using FunctionFilter = std::function<void(Image&)>;

inline void apply_function(const std::vector<FunctionFilter>& chain, Image& image)
{
    for (const FunctionFilter& filter : chain) {
        filter(image);
    }
}

} // namespace pixelforge::poly
