# Error messages: `enable_if` against a concept

The filter chain is written twice in [filters.hpp](../../include/pixelforge/filters.hpp): `apply_enable_if` with `std::enable_if` (v6, issue #30) and `apply` with the concept `FilterFor` (issue #31). Both reject the same calls. This note compares what the compiler says when one of them is called wrongly.

[bad_chain.cpp](bad_chain.cpp) holds four wrong calls, and `compare.sh COMPILER`, run from the repository root, compiles each one against both versions. The complete output is in [apple-clang-21.txt](apple-clang-21.txt) and [gcc-15.2.txt](gcc-15.2.txt). Only these two compilers were tried: Apple Clang 21.0.0 on this machine, and g++ 15.2.0 on GitHub's `ubuntu-latest` runner. Other versions may word their messages differently.

| Case | The wrong call |
|---|---|
| 1 | `apply(image, Grayscale{}, NotAFilter{})`: the second type has no `apply` |
| 2 | `apply(image, WrongSignature{})`: its `apply` takes an `int` |
| 3 | `apply(image, NonConstApply{})`: its `apply` is not `const` |
| 4 | `apply(float_image, Grayscale{})`: a filter class, which works on 8-bit images only |

## What each compiler says

All of them reject the call with "no matching function" at the line of the call. They differ in what comes after.

**Apple Clang 21, concept.** The last notes give the failing filter and the reason, for example in case 1:

```
note: candidate template ignored: constraints not satisfied [with T = unsigned char, Filters = <pixelforge::Grayscale, NotAFilter>]
note: because 'FilterFor<NotAFilter, unsigned char>' evaluated to false
note: because 'filter.apply(image)' would be invalid: no member named 'apply' in 'NotAFilter'
```

The reason differs per case: "no viable conversion from 'BasicImage<unsigned char>' to 'int'" (case 2), "'this' argument to member function 'apply' has type 'const NonConstApply', but function is not marked const" (case 3), and "non-const lvalue reference to type 'BasicImage<std::uint8_t>' cannot bind to a value of unrelated type 'BasicImage<float>'" (case 4). Nothing about the cause is left to guess.

**Apple Clang 21, `enable_if`.** One note, with the name of the failing filter but no reason:

```
note: candidate template ignored: requirement 'detail::is_filter_for_v<NotAFilter, unsigned char>' was not satisfied [with T = unsigned char, Filters = <pixelforge::Grayscale, NotAFilter>]
```

It is short and it names the type. To find out why, the reader has to open `is_filter_for` and work out which `decltype` failed.

**g++ 15.2, concept.** It says that the constraints were not satisfied, but it does not say which filter or why:

```
note: constraints not satisfied
note: the expression '(... && FilterFor<Filters, T>) [with Filters = {pixelforge::Grayscale, NotAFilter}; T = unsigned char]' evaluated to 'false'
```

`FilterFor<T>... Filters` is expanded into one fold expression over the whole pack, and g++ reports the fold as a unit. In case 1 the message does not say whether `NotAFilter` or `Grayscale` was the problem; the reader has to check each type in the list. The concept's name is in the message, so it leads to the right place, but not to the cause.

**g++ 15.2, `enable_if`.** The message ends inside the standard library:

```
/usr/include/c++/15/type_traits: In substitution of 'template<bool _Cond, class _Tp> using std::enable_if_t = typename std::enable_if::type [with bool _Cond = false; _Tp = void]':
.../type_traits:2838:11: error: no type named 'type' in 'struct std::enable_if<false, void>'
```

This is the classic `enable_if` message. It is true, but it is about `enable_if`'s implementation and says nothing about filters. The name `is_filter_for_v` appears only in the printed signature of the candidate, and the last line of the error points into `<type_traits>`.

## What it shows

- **Where the check is written changes what the compiler can say.** A concept has a name and a `requires` expression, so Clang can print "the concept was false because this expression is invalid". With `enable_if` the condition is a `bool` that has already been computed, and the compiler can only say that it was false.
- **The concept was clearly better on Clang** (it names the filter and the reason) and **somewhat better on g++**: it says "constraints not satisfied" and names the concept, instead of an error inside `<type_traits>`. But in g++ 15.2 it did not name the failing filter, because of the fold over the pack. The difference between the compilers was larger than the difference between the two techniques.
- **Both versions reject the call where it is made**, with "no matching function" at the line of the call, which was the point of both.
- **The concept version is also shorter to write.** The constraint is `FilterFor<T>... Filters`, where the `enable_if` one needs a trait class with `std::void_t`, a `_v` variable template and a return type. The tests in [blur_test.cpp](../../tests/blur_test.cpp) show that the two accept and reject exactly the same types.

Not tried: a version that checks the filters one by one, for example with a separate overload per filter, which might make g++ name the failing one.
