#include "prism/StereoAnalyzer.h"
#include "TestSignals.h"

#include <gtest/gtest.h>

using namespace prism;
using namespace prism::test;

namespace
{
    StereoAnalyzer run (const std::vector<float>& l, const std::vector<float>& r)
    {
        StereoAnalyzer s;
        s.prepare (48000.0);
        processInBlocks (l.size(), [&] (size_t pos, int n) { s.process (l.data() + pos, r.data() + pos, n); });
        return s;
    }
} // namespace

TEST (Stereo, MonoIsFullyCorrelated)
{
    const auto x = whiteNoise (48000, 0.5f, 2u);
    const auto s = run (x, x);
    EXPECT_NEAR (s.getCorrelation(), 1.0, 1.0e-6);
    EXPECT_NEAR (s.getWidth(), 0.0, 1.0e-6);
    EXPECT_NEAR (s.getBalanceDb(), 0.0, 1.0e-6);
}

TEST (Stereo, PolarityFlipIsAntiCorrelated)
{
    const auto x = whiteNoise (48000, 0.5f, 3u);
    auto y = x;
    for (auto& v : y)
        v = -v;
    EXPECT_NEAR (run (x, y).getCorrelation(), -1.0, 1.0e-6);
}

TEST (Stereo, IndependentNoiseIsUncorrelated)
{
    const auto s = run (whiteNoise (96000, 0.5f, 4u), whiteNoise (96000, 0.5f, 5u));
    EXPECT_NEAR (s.getCorrelation(), 0.0, 0.1);
    EXPECT_NEAR (s.getWidth(), 1.0, 0.2);
}

TEST (Stereo, BalanceInDb)
{
    const auto x = whiteNoise (48000, 0.5f, 6u);
    auto quieter = x;
    for (auto& v : quieter)
        v *= 0.5f;
    EXPECT_NEAR (run (x, quieter).getBalanceDb(), 6.02, 0.01);
}

TEST (Stereo, SilenceIsNeutral)
{
    std::vector<float> z (4800, 0.0f);
    const auto s = run (z, z);
    EXPECT_EQ (s.getCorrelation(), 0.0);
    EXPECT_EQ (s.getBalanceDb(), 0.0);
    EXPECT_EQ (s.getWidth(), 0.0);
}
