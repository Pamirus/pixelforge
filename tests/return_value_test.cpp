// Which ways of returning an image from a function copy it, move it, or do
// neither. CountedImage counts every copy and move, so each test states
// exactly what happens:
//
//   return CountedImage(4, 4);       nothing: guaranteed copy elision (RVO)
//   return image;                    nothing: NRVO, where the compiler builds
//                                    the named image in the caller's object
//   return std::move(image);         one move: std::move turns NRVO off
//   two named images, one returned   one move: NRVO cannot cover both
//   return a by-value parameter      one move: parameters are never elided
//   return first ? a : b;            one copy: not a plain name, so no move
//   return a const reference         one copy
//   existing = make_image();         one move assignment: elision only
//                                    applies when an object is created

#include <gtest/gtest.h>

#include "pixelforge/counted_image.hpp"

#include <utility>

using pixelforge::CopyMoveCounts;
using pixelforge::CountedImage;

namespace {

class ReturnValueTest : public ::testing::Test {
protected:
    void SetUp() override { CountedImage::reset_counts(); }
};

CountedImage make_temporary()
{
    return CountedImage(4, 4);
}

CountedImage make_named()
{
    CountedImage image(4, 4);
    image.image().data()[0] = 1; // some work on the named image first
    return image;
}

CountedImage make_named_with_std_move()
{
    CountedImage image(4, 4);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpessimizing-move" // the compiler rightly warns
    return std::move(image);
#pragma GCC diagnostic pop
}

CountedImage pick_one_of_two(bool first)
{
    CountedImage a(2, 2);
    CountedImage b(4, 4);
    if (first) {
        return a;
    }
    return b;
}

CountedImage pass_through(CountedImage image)
{
    return image;
}

CountedImage pick_with_conditional(bool first)
{
    CountedImage a(2, 2);
    CountedImage b(4, 4);
    return first ? a : b;
}

CountedImage copy_of(const CountedImage& image)
{
    return image;
}

} // namespace

// Since C++17 a temporary that initializes an object of the same type is
// never copied or moved; the standard requires it.
TEST_F(ReturnValueTest, TemporaryIsNeverCopiedOrMoved)
{
    const CountedImage image = make_temporary();
    EXPECT_EQ(image.image().width(), 4);
    EXPECT_EQ(CountedImage::counts(), CopyMoveCounts{});
}

// NRVO is allowed but not required. GCC and Clang do it even without
// optimization. Without it, the return would still move, never copy.
TEST_F(ReturnValueTest, NamedImageIsElidedByNrvo)
{
    const CountedImage image = make_named();
    EXPECT_EQ(image.image().data()[0], 1);
    EXPECT_EQ(CountedImage::counts(), CopyMoveCounts{});
}

TEST_F(ReturnValueTest, StdMoveOnReturnCostsAMove)
{
    const CountedImage image = make_named_with_std_move();
    EXPECT_EQ(CountedImage::counts(), (CopyMoveCounts{.move_constructions = 1}));
}

// Both images exist when the function decides which one to return, so neither
// can be built in the caller's object. The returned one is moved out.
TEST_F(ReturnValueTest, OneOfTwoNamedImagesIsMoved)
{
    const CountedImage image = pick_one_of_two(false);
    EXPECT_EQ(image.image().width(), 4);
    EXPECT_EQ(CountedImage::counts(), (CopyMoveCounts{.move_constructions = 1}));
}

// The caller creates a by-value parameter, so it cannot also be the return
// value. Returning it moves it.
TEST_F(ReturnValueTest, ReturnedParameterIsMoved)
{
    const CountedImage from_temporary = pass_through(CountedImage(4, 4));
    EXPECT_EQ(CountedImage::counts(), (CopyMoveCounts{.move_constructions = 1}));

    CountedImage::reset_counts();
    const CountedImage original(4, 4);
    const CountedImage from_named = pass_through(original); // copied into the parameter
    EXPECT_EQ(CountedImage::counts(),
              (CopyMoveCounts{.copy_constructions = 1, .move_constructions = 1}));
}

// Only a plain name in a return statement is moved automatically. A
// conditional expression is not a name, so the chosen image is copied.
TEST_F(ReturnValueTest, ConditionalExpressionIsCopied)
{
    const CountedImage image = pick_with_conditional(true);
    EXPECT_EQ(image.image().width(), 2);
    EXPECT_EQ(CountedImage::counts(), (CopyMoveCounts{.copy_constructions = 1}));
}

TEST_F(ReturnValueTest, ConstReferenceIsCopied)
{
    const CountedImage original(4, 4);
    const CountedImage image = copy_of(original);
    EXPECT_NE(image.image().data(), original.image().data());
    EXPECT_EQ(CountedImage::counts(), (CopyMoveCounts{.copy_constructions = 1}));
}

// Elision only happens when the returned value creates a new object. An
// existing object gets a move assignment instead.
TEST_F(ReturnValueTest, AssigningToAnExistingImageMoves)
{
    CountedImage image;
    image = make_temporary();
    EXPECT_EQ(CountedImage::counts(), (CopyMoveCounts{.move_assignments = 1}));
}
