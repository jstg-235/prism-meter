#pragma once

#include <prism/SpectrumAnalyzer.h>

#include <juce_gui_basics/juce_gui_basics.h>

#include <vector>

//==============================================================================
/// A labelled bank of vertical bars sharing one scale (for VU, PPM, K-System).
class BarMeterBank final : public juce::Component
{
public:
    struct Scale
    {
        float minValue = -40.0f;
        float maxValue = 3.0f;
        float warnFrom = 0.0f;   // values at or above draw in warn colour
        float overFrom = 3.0f;   // values at or above draw in over colour
        std::vector<float> ticks;
        juce::String unit;
    };

    BarMeterBank (juce::String title, int numBars);

    void setScale (Scale newScale);
    void setTitle (juce::String newTitle);
    void setBarLabel (int index, juce::String label);

    /// Values are in display units (already offset by any reference level).
    void setValue (int index, float value);
    void setNumBarsVisible (int n);

    void paint (juce::Graphics&) override;

private:
    float toY (float value, juce::Rectangle<float> area) const noexcept;

    juce::String title;
    Scale scale;
    std::vector<float> values, holds;
    std::vector<int> holdAge;
    std::vector<juce::String> labels;
    int visibleBars;
};

//==============================================================================
/// Loudness numbers and short-term / momentary bars against the chosen target.
class LoudnessPanel final : public juce::Component
{
public:
    struct Readings
    {
        float momentary = -150.0f, shortTerm = -150.0f, integrated = -150.0f;
        float loudnessRange = 0.0f, momentaryMax = -150.0f, shortTermMax = -150.0f;
        float truePeakMax = -150.0f, samplePeakMax = -150.0f;
        float target = -14.0f, truePeakCeiling = -1.0f;
    };

    void setReadings (const Readings& r);
    void paint (juce::Graphics&) override;

private:
    Readings readings;
};

//==============================================================================
/// Dual-needle peak and average gauge (our design): two needles on one arc so
/// the gap between them shows the crest factor.
class NeedleGauge final : public juce::Component
{
public:
    void setValues (float peakDb, float averageDb);
    void paint (juce::Graphics&) override;

private:
    float peak = -60.0f, average = -60.0f;
};

//==============================================================================
/// Correlation bar plus balance and width readouts.
class StereoPanel final : public juce::Component
{
public:
    void setValues (float correlation, float balanceDb, float width);
    void paint (juce::Graphics&) override;

private:
    float correlation = 0.0f, balance = 0.0f, width = 0.0f;
};

//==============================================================================
/// Log-frequency FFT spectrum with a peak-hold trace and 4.5 dB/oct tilt option.
class SpectrumView final : public juce::Component
{
public:
    SpectrumView();

    void prepare (double sampleRate);

    /// Feed newly arrived mono samples (message thread).
    void pushSamples (const float* samples, int numSamples);

    void setTiltEnabled (bool shouldTilt) { tilt = shouldTilt; repaint(); }
    void resetPeakHold() { analyzer.reset(); }

    void paint (juce::Graphics&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override { resetPeakHold(); }

private:
    static constexpr int fftOrder = 13; // 8192 points
    static constexpr int hopSize = 2048;

    float frequencyToX (float hz, float width) const noexcept;
    float tiltAt (float hz) const noexcept;

    prism::SpectrumAnalyzer analyzer;
    std::vector<float> ring, frame;
    int ringWrite = 0;
    int samplesSinceFrame = 0;
    double sampleRate = 48000.0;
    bool tilt = true;
};
