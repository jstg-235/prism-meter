#include "prism/SpectrumAnalyzer.h"
#include "TestSignals.h"

#include <gtest/gtest.h>

using namespace prism;
using namespace prism::test;

TEST (Spectrum, FullScaleSineOnABinReadsZeroDb)
{
    const double fs = 48000.0;
    SpectrumAnalyzer a;
    a.prepare (12, fs); // 4096 points
    a.setAveraging (0.0f);

    const int bin = 100;
    const double hz = a.getBinFrequency (bin);
    std::vector<float> x;
    appendSine (x, fs, hz, 0.0, static_cast<double> (a.getFftSize()) / fs);
    a.processFrame (x.data());

    const auto& mags = a.getMagnitudesDb();
    EXPECT_NEAR (mags[bin], 0.0, 0.05);
    EXPECT_NEAR (a.getLevelAtFrequency (hz), 0.0, 0.05);
    EXPECT_LT (mags[bin + 20], -80.0);
    EXPECT_LT (mags[bin - 20], -80.0);
}

TEST (Spectrum, FftMatchesDirectDft)
{
    Fft fft;
    fft.prepare (6);
    const int n = fft.getSize();
    const auto noise = whiteNoise (static_cast<size_t> (n), 1.0f, 7u);

    std::vector<std::complex<float>> data (static_cast<size_t> (n));
    for (int i = 0; i < n; ++i)
        data[static_cast<size_t> (i)] = { noise[static_cast<size_t> (i)], 0.0f };
    fft.forward (data.data());

    for (int k = 0; k < n; ++k)
    {
        std::complex<double> sum;
        for (int i = 0; i < n; ++i)
            sum += static_cast<double> (noise[static_cast<size_t> (i)]) * std::polar (1.0, -2.0 * std::numbers::pi * k * i / n);

        EXPECT_NEAR (data[static_cast<size_t> (k)].real(), sum.real(), 1.0e-4);
        EXPECT_NEAR (data[static_cast<size_t> (k)].imag(), sum.imag(), 1.0e-4);
    }
}

TEST (Spectrum, PeakHoldKeepsTheMaximum)
{
    const double fs = 48000.0;
    SpectrumAnalyzer a;
    a.prepare (10, fs);
    a.setAveraging (0.9f);

    std::vector<float> loud, quiet;
    appendSine (loud, fs, a.getBinFrequency (50), -6.0, 1024.0 / fs);
    appendSine (quiet, fs, a.getBinFrequency (50), -40.0, 1024.0 / fs);
    a.processFrame (loud.data());
    a.processFrame (quiet.data());

    EXPECT_NEAR (a.getPeakHoldDb()[50], -6.0, 0.1);
    EXPECT_LT (a.getMagnitudesDb()[50], -6.0);
}
