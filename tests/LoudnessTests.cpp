// Loudness tests built from the EBU Tech 3341 and Tech 3342 test signal
// descriptions (generated here rather than loaded from the EBU files).

#include "prism/LoudnessMeter.h"
#include "TestSignals.h"

#include <gtest/gtest.h>

using namespace prism;
using namespace prism::test;

namespace
{
    struct Segment
    {
        double levelDbfs; // a level below -200 means silence
        double seconds;
    };

    std::vector<float> stereoSineProgram (double sampleRate, std::initializer_list<Segment> segments)
    {
        std::vector<float> mono;
        for (const auto& s : segments)
        {
            if (s.levelDbfs < -200.0)
                appendSilence (mono, sampleRate, s.seconds);
            else
                appendSine (mono, sampleRate, 1000.0, s.levelDbfs, s.seconds);
        }
        return mono;
    }

    LoudnessMeter runStereo (const std::vector<float>& mono, double sampleRate)
    {
        LoudnessMeter meter;
        meter.prepare (sampleRate, 2);

        processInBlocks (mono.size(), [&] (size_t pos, int n)
        {
            const float* chans[2] = { mono.data() + pos, mono.data() + pos };
            meter.process (chans, 2, n);
        });

        return meter;
    }
} // namespace

class LoudnessSampleRates : public ::testing::TestWithParam<double> {};

// Tech 3341 case 1: stereo 1 kHz at -23 dBFS reads -23.0 LUFS (M, S and I).
TEST_P (LoudnessSampleRates, Tech3341Case1)
{
    const double fs = GetParam();
    const auto meter = runStereo (stereoSineProgram (fs, { { -23.0, 20.0 } }), fs);
    EXPECT_NEAR (meter.getMomentary(), -23.0, 0.1);
    EXPECT_NEAR (meter.getShortTerm(), -23.0, 0.1);
    EXPECT_NEAR (meter.getIntegrated(), -23.0, 0.1);
}

INSTANTIATE_TEST_SUITE_P (Rates, LoudnessSampleRates, ::testing::Values (44100.0, 48000.0, 88200.0, 96000.0, 192000.0));

// Tech 3341 case 2: -33 dBFS reads -33.0 LUFS.
TEST (Loudness, Tech3341Case2)
{
    const auto meter = runStereo (stereoSineProgram (48000.0, { { -33.0, 20.0 } }), 48000.0);
    EXPECT_NEAR (meter.getMomentary(), -33.0, 0.1);
    EXPECT_NEAR (meter.getShortTerm(), -33.0, 0.1);
    EXPECT_NEAR (meter.getIntegrated(), -33.0, 0.1);
}

// Tech 3341 case 3: relative gate removes the quiet parts.
TEST (Loudness, Tech3341Case3RelativeGate)
{
    const auto meter = runStereo (stereoSineProgram (48000.0, { { -36.0, 10.0 }, { -23.0, 60.0 }, { -36.0, 10.0 } }), 48000.0);
    EXPECT_NEAR (meter.getIntegrated(), -23.0, 0.1);
}

// Tech 3341 case 4: absolute and relative gates together.
TEST (Loudness, Tech3341Case4AbsoluteAndRelativeGate)
{
    const auto meter = runStereo (stereoSineProgram (48000.0, { { -72.0, 10.0 }, { -36.0, 10.0 }, { -23.0, 60.0 },
                                                                { -36.0, 10.0 }, { -72.0, 10.0 } }), 48000.0);
    EXPECT_NEAR (meter.getIntegrated(), -23.0, 0.1);
}

// Tech 3341 case 5: gating with a level step inside the measurement.
TEST (Loudness, Tech3341Case5)
{
    const auto meter = runStereo (stereoSineProgram (48000.0, { { -26.0, 20.0 }, { -20.0, 20.1 }, { -26.0, 20.0 } }), 48000.0);
    EXPECT_NEAR (meter.getIntegrated(), -23.0, 0.1);
}

TEST (Loudness, SilenceReadsNothing)
{
    const auto meter = runStereo (stereoSineProgram (48000.0, { { -999.0, 5.0 } }), 48000.0);
    EXPECT_EQ (meter.getIntegrated(), kSilenceDb);
    EXPECT_EQ (meter.getMomentary(), kSilenceDb);
    EXPECT_DOUBLE_EQ (meter.getLoudnessRange(), 0.0);
}

TEST (Loudness, ResetIntegratedForgetsHistory)
{
    LoudnessMeter meter;
    meter.prepare (48000.0, 2);

    auto feed = [&] (const std::vector<float>& mono)
    {
        processInBlocks (mono.size(), [&] (size_t pos, int n)
        {
            const float* chans[2] = { mono.data() + pos, mono.data() + pos };
            meter.process (chans, 2, n);
        });
    };

    feed (stereoSineProgram (48000.0, { { -15.0, 10.0 } }));
    EXPECT_NEAR (meter.getIntegrated(), -15.0, 0.1);

    meter.resetIntegrated();
    feed (stereoSineProgram (48000.0, { { -30.0, 10.0 } }));
    EXPECT_NEAR (meter.getIntegrated(), -30.0, 0.1);
    EXPECT_NEAR (meter.getMomentaryMax(), -30.0, 0.1);
}

TEST (Loudness, SurroundWeights)
{
    // One 1 kHz tone on Ls only (weight 1.41) reads 1.5 dB above the same tone on L.
    const double fs = 48000.0;
    std::vector<float> tone;
    appendSine (tone, fs, 1000.0, -20.0, 5.0);
    std::vector<float> silence (tone.size(), 0.0f);

    auto measure = [&] (int toneChannel)
    {
        LoudnessMeter meter;
        meter.prepare (fs, 6);
        const float* chans[6];
        for (int c = 0; c < 6; ++c)
            chans[c] = (c == toneChannel ? tone : silence).data();
        meter.process (chans, 6, static_cast<int> (tone.size()));
        return meter.getIntegrated();
    };

    EXPECT_NEAR (measure (4) - measure (0), 10.0 * std::log10 (1.41), 0.01);
    EXPECT_EQ (measure (3), kSilenceDb); // LFE is excluded
}

// EBU Tech 3342 loudness range cases (expected LRA +/- 1 LU).
TEST (LoudnessRange, Tech3342Case1)
{
    const auto meter = runStereo (stereoSineProgram (48000.0, { { -20.0, 20.0 }, { -30.0, 20.0 } }), 48000.0);
    EXPECT_NEAR (meter.getLoudnessRange(), 10.0, 1.0);
}

TEST (LoudnessRange, Tech3342Case2)
{
    const auto meter = runStereo (stereoSineProgram (48000.0, { { -20.0, 20.0 }, { -15.0, 20.0 } }), 48000.0);
    EXPECT_NEAR (meter.getLoudnessRange(), 5.0, 1.0);
}

TEST (LoudnessRange, Tech3342Case3)
{
    const auto meter = runStereo (stereoSineProgram (48000.0, { { -40.0, 20.0 }, { -20.0, 20.0 } }), 48000.0);
    EXPECT_NEAR (meter.getLoudnessRange(), 20.0, 1.0);
}

TEST (LoudnessRange, Tech3342Case4)
{
    const auto meter = runStereo (stereoSineProgram (48000.0, { { -50.0, 20.0 }, { -35.0, 20.0 }, { -20.0, 20.0 },
                                                                { -35.0, 20.0 }, { -50.0, 20.0 } }), 48000.0);
    EXPECT_NEAR (meter.getLoudnessRange(), 15.0, 1.0);
}
