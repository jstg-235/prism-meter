#include <prism/LoudnessHistory.h>

#include <gtest/gtest.h>

#include <vector>

using prism::LoudnessHistory;

TEST (LoudnessHistory, StartsEmpty)
{
    LoudnessHistory h (8);
    std::vector<float> out (8);
    std::uint64_t first = 99;
    EXPECT_EQ (h.size(), 0u);
    EXPECT_EQ (h.readLatest (out.data(), 8, first), 0);
    EXPECT_EQ (first, 0u);
}

TEST (LoudnessHistory, ReturnsNewestOldestFirst)
{
    LoudnessHistory h (8);
    for (int i = 0; i < 5; ++i)
        h.push (static_cast<float> (i));

    std::vector<float> out (8);
    std::uint64_t first = 0;
    ASSERT_EQ (h.readLatest (out.data(), 3, first), 3);
    EXPECT_EQ (first, 2u);
    EXPECT_FLOAT_EQ (out[0], 2.0f);
    EXPECT_FLOAT_EQ (out[2], 4.0f);
}

TEST (LoudnessHistory, WrapsAndKeepsOnlyCapacity)
{
    LoudnessHistory h (4);
    for (int i = 0; i < 10; ++i)
        h.push (static_cast<float> (i));

    EXPECT_EQ (h.size(), 10u);
    std::vector<float> out (8);
    std::uint64_t first = 0;
    ASSERT_EQ (h.readLatest (out.data(), 8, first), 4);
    EXPECT_EQ (first, 6u);
    EXPECT_FLOAT_EQ (out[0], 6.0f);
    EXPECT_FLOAT_EQ (out[3], 9.0f);
}

TEST (LoudnessHistory, ClearStartsANewRun)
{
    LoudnessHistory h (8);
    for (int i = 0; i < 5; ++i)
        h.push (static_cast<float> (i));
    h.clear();
    EXPECT_EQ (h.size(), 0u);

    h.push (42.0f);
    std::vector<float> out (8);
    std::uint64_t first = 7;
    ASSERT_EQ (h.readLatest (out.data(), 8, first), 1);
    EXPECT_EQ (first, 0u);
    EXPECT_FLOAT_EQ (out[0], 42.0f);
}
