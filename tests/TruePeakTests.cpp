#include "prism/TruePeakMeter.h"
#include "TestSignals.h"

#include <gtest/gtest.h>
#include <numbers>

using namespace prism;
using namespace prism::test;

namespace
{
    TruePeakMeter run (const std::vector<float>& x, double fs)
    {
        TruePeakMeter m;
        m.prepare (fs);
        processInBlocks (x.size(), [&] (size_t pos, int n) { m.process (x.data() + pos, n); });
        return m;
    }
} // namespace

TEST (TruePeak, OversamplingFactorFollowsSampleRate)
{
    TruePeakMeter m;
    m.prepare (44100.0);  EXPECT_EQ (m.getOversamplingFactor(), 4);
    m.prepare (48000.0);  EXPECT_EQ (m.getOversamplingFactor(), 4);
    m.prepare (96000.0);  EXPECT_EQ (m.getOversamplingFactor(), 2);
    m.prepare (192000.0); EXPECT_EQ (m.getOversamplingFactor(), 1);
}

// A sine at fs/4 sampled 45 degrees off its peaks: samples sit at -3.01 dBFS,
// the reconstructed waveform peaks at 0 dBTP.
TEST (TruePeak, FindsInterSamplePeak)
{
    const double fs = 48000.0;
    std::vector<float> x;
    appendSine (x, fs, fs / 4.0, 0.0, 1.0, std::numbers::pi / 4.0);

    const auto m = run (x, fs);
    EXPECT_NEAR (gainToDb (m.getSamplePeak()), -3.01, 0.05);
    EXPECT_NEAR (gainToDb (m.getTruePeak()), 0.0, 0.2);
}

TEST (TruePeak, LowFrequencySineReadsItsPeak)
{
    for (double fs : { 44100.0, 48000.0, 96000.0 })
    {
        std::vector<float> x;
        appendSine (x, fs, 997.0, -6.0, 1.0);
        const auto m = run (x, fs);
        EXPECT_NEAR (gainToDb (m.getTruePeak()), -6.0, 0.1) << "fs " << fs;
    }
}

// Passband accuracy across the audible band (BS.1770 asks for under 0.3 dB of under-read).
TEST (TruePeak, HighFrequencyAccuracy)
{
    const double fs = 48000.0;
    for (double hz : { 5000.0, 10000.0, 15000.0, 19000.0 })
    {
        for (double phase : { 0.0, 0.7, 1.9 })
        {
            std::vector<float> x;
            appendSine (x, fs, hz, -6.0, 0.5, phase);

            // Skip the switch-on transient: an abrupt start rings in any band-limited interpolator.
            TruePeakMeter m;
            m.prepare (fs);
            m.process (x.data(), 2400);
            m.resetPeaks();
            m.process (x.data() + 2400, static_cast<int> (x.size()) - 2400);
            EXPECT_NEAR (gainToDb (m.getTruePeak()), -6.0, 0.3) << hz << " Hz, phase " << phase;
        }
    }
}

TEST (TruePeak, NeverBelowSamplePeak)
{
    const auto x = whiteNoise (48000, 0.8f, 1u);
    const auto m = run (x, 48000.0);
    EXPECT_GE (m.getTruePeak(), m.getSamplePeak());
}

TEST (TruePeak, ResetClearsPeaks)
{
    std::vector<float> x;
    appendSine (x, 48000.0, 1000.0, 0.0, 0.1);
    auto m = run (x, 48000.0);
    m.resetPeaks();
    EXPECT_EQ (m.getTruePeak(), 0.0f);
    EXPECT_EQ (m.getSamplePeak(), 0.0f);
}
