#include "prism/KWeighting.h"

#include <gtest/gtest.h>

using namespace prism;

// Reference coefficients printed in ITU-R BS.1770 for 48 kHz.
TEST (KWeighting, ShelfMatchesRecommendationAt48k)
{
    const auto c = KWeighting::makeShelf (48000.0);
    EXPECT_NEAR (c.b0, 1.53512485958697, 1.0e-8);
    EXPECT_NEAR (c.b1, -2.69169618940638, 1.0e-8);
    EXPECT_NEAR (c.b2, 1.19839281085285, 1.0e-8);
    EXPECT_NEAR (c.a1, -1.69065929318241, 1.0e-8);
    EXPECT_NEAR (c.a2, 0.73248077421585, 1.0e-8);
}

TEST (KWeighting, HighPassMatchesRecommendationAt48k)
{
    const auto c = KWeighting::makeHighPass (48000.0);
    EXPECT_DOUBLE_EQ (c.b0, 1.0);
    EXPECT_DOUBLE_EQ (c.b1, -2.0);
    EXPECT_DOUBLE_EQ (c.b2, 1.0);
    EXPECT_NEAR (c.a1, -1.99004745483398, 1.0e-8);
    EXPECT_NEAR (c.a2, 0.99007225036621, 1.0e-8);
}
