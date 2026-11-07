#pragma once

#include <prism/SpectrumAnalyzer.h>

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <vector>

//==============================================================================
/// A bank of segmented LED-style bars sharing one scale (VU, PPM, K-System).
/// Segments light green below the warn level, yellow up to the over level and
/// red above it; unlit segments stay faintly visible like a hardware meter.
class BarMeterBank final : public juce::Component
{
public:
    struct Scale
    {
        float minValue = -40.0f;
        float maxValue = 3.0f;
        float warnFrom = 0.0f;
        float overFrom = 3.0f;
        std::vector<float> ticks;
        juce::String unit;
    };

    BarMeterBank (juce::String title, juce::Colour accent, int numBars);

    void setScale (Scale newScale);
    void setTitle (juce::String newTitle);
    void setBarLabel (int index, juce::String label);
    void setValue (int index, float value);
    void setNumBarsVisible (int n);

    void paint (juce::Graphics&) override;

private:
    float toNorm (float value) const noexcept;
    juce::Colour zoneColour (float value) const noexcept;

    juce::String title;
    juce::Colour accent;
    Scale scale;
    std::vector<float> values, holds;
    std::vector<int> holdAge;
    std::vector<juce::String> labels;
    int visibleBars;
};

//==============================================================================
/// Loudness readouts, segmented momentary / short-term bars and a scrolling
/// short-term history against the target.
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

    /// Call at a steady rate (10 Hz) to extend the history graph.
    void pushHistory (float shortTermLufs, float momentaryLufs);
    void clearHistory();

    void paint (juce::Graphics&) override;

private:
    static constexpr int historyLength = 600; // 60 s at 10 Hz

    Readings readings;
    std::array<float, historyLength> shortHistory {}, momentaryHistory {};
    int historyWrite = 0;
    int historyCount = 0;
};

//==============================================================================
/// Dual-needle peak and average gauge on an ivory analogue face (our design).
class NeedleGauge final : public juce::Component
{
public:
    void setValues (float peakDb, float averageDb);
    void paint (juce::Graphics&) override;

private:
    float peak = -60.0f, average = -60.0f;
};

//==============================================================================
/// Vectorscope (mid up, side across) with phosphor persistence, plus the
/// correlation bar and balance / width readouts.
class PhasePanel final : public juce::Component
{
public:
    PhasePanel();

    void setValues (float correlation, float balanceDb, float width);
    void pushSamples (const float* left, const float* right, int numSamples);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void fadeTrace();

    juce::Image trace;
    float correlation = 0.0f, balance = 0.0f, width = 0.0f;
    float autoGain = 1.0f;
};

//==============================================================================
/// Log-frequency spectrum with a frequency-coloured fill, peak hold and tilt,
/// and a scrolling spectrogram below it.
class SpectrumView final : public juce::Component
{
public:
    SpectrumView();

    void prepare (double sampleRate);
    void pushSamples (const float* samples, int numSamples);

    void setTiltEnabled (bool shouldTilt) { tilt = shouldTilt; repaint(); }
    void resetPeakHold() { analyzer.reset(); }

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDoubleClick (const juce::MouseEvent&) override { resetPeakHold(); }

private:
    static constexpr int fftOrder = 13; // 8192 points
    static constexpr int hopSize = 2048;
    static constexpr int spectrogramRows = 160;

    float tiltAt (float hz) const noexcept;
    float levelBetween (float hzLo, float hzHi, bool peak) const noexcept;
    void addSpectrogramColumn();

    prism::SpectrumAnalyzer analyzer;
    std::vector<float> ring, frame;
    int ringWrite = 0;
    int samplesSinceFrame = 0;
    double sampleRate = 48000.0;
    bool tilt = true;

    juce::Image spectrogram;
};
