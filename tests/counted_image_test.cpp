// Tests for pixelforge::CountedImage.

#include <gtest/gtest.h>

#include "pixelforge/counted_image.hpp"

#include <cstdint>
#include <type_traits>
#include <utility>
#include <vector>

using pixelforge::CopyMoveCounts;
using pixelforge::CountedImage;

static_assert(std::is_nothrow_move_constructible_v<CountedImage>);
static_assert(std::is_nothrow_move_assignable_v<CountedImage>);

namespace {

// Every test starts from zero, also when the tests run in one process.
class CountedImageTest : public ::testing::Test {
protected:
    void SetUp() override { CountedImage::reset_counts(); }
};

} // namespace

TEST_F(CountedImageTest, ConstructionIsNotCounted)
{
    const CountedImage empty;
    const CountedImage black(4, 4);
    const CountedImage wrapped(pixelforge::Image(2, 2));
    EXPECT_EQ(CountedImage::counts(), CopyMoveCounts{});
}

TEST_F(CountedImageTest, CountsCopyConstruction)
{
    const CountedImage original(4, 4);
    const CountedImage copy(original);

    EXPECT_EQ(CountedImage::counts(), (CopyMoveCounts{.copy_constructions = 1}));
    EXPECT_NE(copy.image().data(), original.image().data()); // still a deep copy
}

TEST_F(CountedImageTest, CountsCopyAssignment)
{
    const CountedImage source(4, 4);
    CountedImage target;
    target = source;

    EXPECT_EQ(CountedImage::counts(), (CopyMoveCounts{.copy_assignments = 1}));
    EXPECT_EQ(target.image().width(), 4);
}

TEST_F(CountedImageTest, CountsMoveConstruction)
{
    CountedImage source(4, 4);
    const std::uint8_t* pixels = source.image().data();
    const CountedImage target(std::move(source));

    EXPECT_EQ(CountedImage::counts(), (CopyMoveCounts{.move_constructions = 1}));
    EXPECT_EQ(target.image().data(), pixels);
    EXPECT_TRUE(source.image().empty());
}

TEST_F(CountedImageTest, CountsMoveAssignment)
{
    CountedImage source(4, 4);
    CountedImage target;
    target = std::move(source);

    EXPECT_EQ(CountedImage::counts(), (CopyMoveCounts{.move_assignments = 1}));
    EXPECT_TRUE(source.image().empty());
}

TEST_F(CountedImageTest, ResetSetsCountsToZero)
{
    const CountedImage original(2, 2);
    const CountedImage copy(original);
    ASSERT_EQ(CountedImage::counts().copies(), 1);

    CountedImage::reset_counts();
    EXPECT_EQ(CountedImage::counts(), CopyMoveCounts{});
}

// When a vector runs out of room it moves its elements to a larger buffer, but
// only if the move cannot throw; otherwise it copies them. CountedImage's move
// is noexcept, so growing the vector moves and never copies.
TEST_F(CountedImageTest, GrowingAVectorMovesInsteadOfCopying)
{
    std::vector<CountedImage> images;
    for (int i = 0; i < 5; ++i) {
        images.emplace_back(4, 4);
    }

    EXPECT_EQ(CountedImage::counts().copies(), 0) << CountedImage::counts();
    EXPECT_GT(CountedImage::counts().move_constructions, 0) << CountedImage::counts();
}
