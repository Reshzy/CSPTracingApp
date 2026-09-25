#include <gtest/gtest.h>

bool IsValidControlViewport(int width, int height) noexcept;

TEST(ViewportPolicy, RejectsZeroWidth)
{
    EXPECT_FALSE(IsValidControlViewport(0, 480));
}

TEST(ViewportPolicy, RejectsZeroHeight)
{
    EXPECT_FALSE(IsValidControlViewport(640, 0));
}

TEST(ViewportPolicy, RejectsZeroWidthAndHeight)
{
    EXPECT_FALSE(IsValidControlViewport(0, 0));
}

TEST(ViewportPolicy, RejectsNegativeWidth)
{
    EXPECT_FALSE(IsValidControlViewport(-1, 480));
}

TEST(ViewportPolicy, AcceptsPositiveDimensions)
{
    EXPECT_TRUE(IsValidControlViewport(640, 480));
}
