#include "prism/Ballistics.h"
#include "TestSignals.h"

#include <gtest/gtest.h>

using namespace prism;
using namespace prism::test;

namespace
{
    template <typename Meter>
    void feed (Meter& m, const std::vector<float>& x)
    {
        processInBlocks (x.size(), [&] (size_t pos, int n) { m.process (x.data() + pos, n); });
    }
} // namespace

TEST (Vu, SteadySineReadsItsPeakLevel)
{
    VuMeter vu;
    vu.prepare (48000.0);
    std::vector<float> x;
    appendSine (x, 48000.0, 1000.0, -18.0, 2.0);
    feed (vu, x);
    EXPECT_NEAR (vu.getReadingDbfs(), -18.0, 0.1);
}

// IEC 60268-17: 99 % of final reading 300 ms after a step.
TEST (Vu, RiseTimeIs300ms)
{
    const double fs = 48000.0;
    VuMeter vu;
    vu.prepare (fs);

    std::vector<float> before;
    appendSine (before, fs, 1000.0, -6.0, 0.290);
    feed (vu, before);
    const double at290 = vu.getReadingDbfs();

    std::vector<float> after;
    appendSine (after, fs, 1000.0, -6.0, 0.020);
    feed (vu, after);
    const double at310 = vu.getReadingDbfs();

    const double target = -6.0 + 20.0 * std::log10 (0.99);
    EXPECT_LT (at290, target + 0.05);
    EXPECT_GT (at310, target - 0.05);
}

namespace
{
    double burstReading (PpmMeter::Type type, double burstSeconds)
    {
        const double fs = 48000.0;
        PpmMeter ppm;
        ppm.setType (type);
        ppm.prepare (fs);

        std::vector<float> x;
        appendSine (x, fs, 5000.0, 0.0, burstSeconds);
        feed (ppm, x);
        return ppm.getReadingDbfs();
    }
} // namespace

// IEC 60268-10 tone-burst responses, 5 kHz, relative to the steady reading.
TEST (Ppm, TypeIIBurstResponse)
{
    EXPECT_NEAR (burstReading (PpmMeter::Type::ebuTypeIIb, 0.010), -4.0, 0.75);
    EXPECT_NEAR (burstReading (PpmMeter::Type::bbcTypeIIa, 0.010), -4.0, 0.75);
    EXPECT_NEAR (burstReading (PpmMeter::Type::ebuTypeIIb, 1.0), 0.0, 0.3);
}

TEST (Ppm, TypeIBurstResponse)
{
    EXPECT_NEAR (burstReading (PpmMeter::Type::dinTypeI, 0.010), -1.0, 0.5);
    EXPECT_NEAR (burstReading (PpmMeter::Type::nordicTypeI, 0.010), -1.0, 0.5);
    EXPECT_NEAR (burstReading (PpmMeter::Type::dinTypeI, 1.0), 0.0, 0.3);
}

TEST (Ppm, FallBackRates)
{
    const double fs = 48000.0;

    for (auto type : { PpmMeter::Type::ebuTypeIIb, PpmMeter::Type::dinTypeI })
    {
        PpmMeter ppm;
        ppm.setType (type);
        ppm.prepare (fs);

        std::vector<float> x;
        appendSine (x, fs, 1000.0, -6.0, 1.0);
        feed (ppm, x);
        const double start = ppm.getReadingDbfs();

        std::vector<float> silence;
        appendSilence (silence, fs, 1.0);
        feed (ppm, silence);

        EXPECT_NEAR (start - ppm.getReadingDbfs(), PpmMeter::fallRateDbPerSecond (type), 0.3);
    }
}

TEST (Rms, Aes17SineReadsPeakLevel)
{
    RmsMeter aes, plain;
    aes.setOptions (0.3, true);
    plain.setOptions (0.3, false);
    aes.prepare (48000.0);
    plain.prepare (48000.0);

    std::vector<float> x;
    appendSine (x, 48000.0, 1000.0, -20.0, 3.0);
    feed (aes, x);
    feed (plain, x);

    EXPECT_NEAR (aes.getReadingDbfs(), -20.0, 0.1);
    EXPECT_NEAR (plain.getReadingDbfs(), -23.01, 0.1);
}

TEST (DualNeedle, NeedlesMeetOnASineAndSplitOnTransients)
{
    const double fs = 48000.0;
    DualNeedleMeter meter;
    meter.prepare (fs);

    std::vector<float> sine;
    appendSine (sine, fs, 1000.0, -12.0, 4.0);
    feed (meter, sine);
    EXPECT_NEAR (meter.getPeakDbfs(), meter.getAverageDbfs(), 0.2);

    // Short clicks: the peak needle jumps, the average barely moves.
    DualNeedleMeter clicks;
    clicks.prepare (fs);
    std::vector<float> x (static_cast<size_t> (fs), 0.0f);
    for (size_t i = 0; i < x.size(); i += 4800)
        x[i] = 0.9f;
    feed (clicks, x);
    EXPECT_GT (clicks.getPeakDbfs() - clicks.getAverageDbfs(), 20.0);
}
