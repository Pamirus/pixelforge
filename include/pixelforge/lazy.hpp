#pragma once

// Lazy filter chains with expression templates.
//
//     Image out = (lazy::from(image) | lazy::Brightness{30} | lazy::Negative{}
//                  | lazy::Brightness{-10}).evaluate();
//
// builds no image and touches no pixel. Each `|` returns a new object whose
// *type* records the filters so far: Chain<uint8_t, Brightness, Negative, ...>.
// The pixels are read, filtered and written once, when the chain is assigned
// to an image (`assign_to`, `evaluate`). The step-by-step way (apply one
// filter, then the next) runs over all 24.9 MB of a 4K image once per filter.
//
// This works because these filters are *pointwise*: the new value of a
// channel depends only on its old value. A blur or a flip needs neighbours or
// other positions, and could not be fused like this without more machinery.

#include "pixelforge/detail/channel_traits.hpp"
#include "pixelforge/image.hpp"

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace pixelforge::lazy {

// A filter in a chain is a small function object from one channel value to
// another. operator() is a template so that one object works on 8-bit and
// float images; the arithmetic is the one of filters.cpp.
struct Negative {
    template <typename T>
    T operator()(T value) const
    {
        return static_cast<T>(detail::ChannelTraits<T>::max - value);
    }
};

struct Brightness {
    // As in brightness(): steps beyond +-255 change nothing more.
    explicit Brightness(int amount) : amount_(std::clamp(amount, -255, 255)) {}

    template <typename T>
    T operator()(T value) const
    {
        using Traits = detail::ChannelTraits<T>;
        using Work = typename Traits::Work;
        return static_cast<T>(
            std::clamp<Work>(value + Traits::from_setting(amount_), 0, Traits::max));
    }

private:
    int amount_;
};

// The image to read, plus the list of filters as a tuple. Chain holds a
// pointer into the source image, not a copy: the source has to outlive it.
template <typename T, typename... Ops>
class Chain {
public:
    Chain(const BasicImage<T>& source, std::tuple<Ops...> ops)
        : source_(&source), ops_(std::move(ops))
    {
    }

    // The new value of channel i: the old one through every filter in order.
    T operator[](std::size_t i) const
    {
        return run(ops_, source_->data()[i]);
    }

    // Add a filter to the end. Nothing is computed.
    template <typename Op>
    Chain<T, Ops..., Op> operator|(Op op) const
    {
        return {*source_, std::tuple_cat(ops_, std::tuple<Op>(std::move(op)))};
    }

    // One pass over the pixels. `out` must have the size of the source; it may
    // be the source itself, because each value is read before it is written.
    void assign_to(BasicImage<T>& out) const
    {
        if (out.width() != source_->width() || out.height() != source_->height()) {
            throw std::invalid_argument("the result image must have the size of the source");
        }
        // The source pointer and the filters go into local variables first.
        // The first version called operator[] here and read source_ and ops_
        // through `this` on every turn. Since a store through a uint8_t* may
        // change any object, Clang had to read them again after each store
        // and gave up on vectorizing the loop ("cannot identify array
        // bounds"); the lazy chain then ran about four times slower than the
        // three passes it was meant to beat. Locals cannot be changed by the
        // store, so the loop sees only the arithmetic.
        const T* const src = source_->data();
        const std::tuple<Ops...> ops = ops_;
        T* const dst = out.data();
        const std::size_t size = out.size();
        for (std::size_t i = 0; i < size; ++i) {
            dst[i] = run(ops, src[i]);
        }
    }

    BasicImage<T> evaluate() const
    {
        BasicImage<T> result(source_->width(), source_->height());
        assign_to(result);
        return result;
    }

private:
    // The value through every filter in order. The fold expression unrolls at
    // compile time, so the loop that calls this has the whole chain in view.
    static T run(const std::tuple<Ops...>& ops, T value)
    {
        std::apply([&](const Ops&... op) { ((value = op(value)), ...); }, ops);
        return value;
    }

    const BasicImage<T>* source_;
    std::tuple<Ops...> ops_;
};

// The start of a chain: an image and no filters yet.
template <typename T>
Chain<T> from(const BasicImage<T>& image)
{
    return {image, {}};
}

} // namespace pixelforge::lazy
