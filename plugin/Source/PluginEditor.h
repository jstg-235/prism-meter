#pragma once

#include "PluginProcessor.h"
#include "ui/MeterViews.h"
#include "ui/Theme.h"

#include <juce_audio_processors/juce_audio_processors.h>

class PrismMeterEditor final : public juce::AudioProcessorEditor,
                               private juce::Timer
{
public:
    explicit PrismMeterEditor (PrismMeterProcessor&);
    ~PrismMeterEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void updateScales();

    struct LabelledCombo
    {
        juce::Label label;
        juce::ComboBox box;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
    };

    void setUpCombo (LabelledCombo& c, const juce::String& name, const juce::StringArray& items, const char* paramId);

    PrismMeterProcessor& processor;

    LoudnessPanel loudnessPanel;
    SpectrumView spectrum;
    PhasePanel phasePanel;
    NeedleGauge needleGauge;
    BarMeterBank vuBank { "VU", Theme::vuAccent, 2 };
    BarMeterBank ppmBank { "PPM", Theme::ppmAccent, 2 };
    BarMeterBank kBank { "K-System", Theme::kAccent, 2 };

    juce::TextButton resetButton { "Reset" };
    juce::ToggleButton tiltButton { "Tilt" };
    LabelledCombo targetCombo, ceilingCombo, vuCombo, ppmCombo, kCombo;

    std::vector<float> spectrumScratch = std::vector<float> (8192);
    std::vector<float> scopeLeft = std::vector<float> (4096), scopeRight = std::vector<float> (4096);
    int tickCount = 0;
    double lastSampleRate = 0.0;
    int lastVuChoice = -1, lastPpmChoice = -1, lastKChoice = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PrismMeterEditor)
};
