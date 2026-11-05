#include "PluginEditor.h"
#include "ui/Theme.h"

namespace
{
    int choiceIndex (juce::AudioProcessorValueTreeState& params, const char* id)
    {
        return static_cast<int> (params.getRawParameterValue (id)->load());
    }
}

PrismMeterEditor::PrismMeterEditor (PrismMeterProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    for (auto* c : std::initializer_list<juce::Component*> { &loudnessPanel, &spectrum, &stereoPanel, &needleGauge,
                                                             &vuBank, &ppmBank, &kBank, &resetButton, &tiltButton })
        addAndMakeVisible (c);

    setUpCombo (targetCombo, "Target", Choices::loudnessTargets, ParamIds::loudnessTarget);
    setUpCombo (ceilingCombo, "Ceiling", Choices::truePeakCeilings, ParamIds::truePeakCeiling);
    setUpCombo (vuCombo, "0 VU =", Choices::vuReferences, ParamIds::vuReference);
    setUpCombo (ppmCombo, "PPM", Choices::ppmTypes, ParamIds::ppmType);
    setUpCombo (kCombo, "K", Choices::kScales, ParamIds::kScale);

    resetButton.setTooltip ("Reset integrated loudness, LRA and peak holds");
    resetButton.onClick = [this]
    {
        processor.requestReset();
        spectrum.resetPeakHold();
    };

    tiltButton.setToggleState (true, juce::dontSendNotification);
    tiltButton.onClick = [this] { spectrum.setTiltEnabled (tiltButton.getToggleState()); };

    for (auto* bank : { &vuBank, &ppmBank, &kBank })
    {
        bank->setBarLabel (0, "L");
        bank->setBarLabel (1, "R");
    }

    setResizable (true, true);
    setResizeLimits (1120, 600, 2400, 1400);
    setSize (1180, 680);

    updateScales();
    startTimerHz (30);
}

PrismMeterEditor::~PrismMeterEditor()
{
    stopTimer();
}

void PrismMeterEditor::setUpCombo (LabelledCombo& c, const juce::String& name, const juce::StringArray& items, const char* paramId)
{
    c.label.setText (name, juce::dontSendNotification);
    c.label.setJustificationType (juce::Justification::centredRight);
    c.label.setColour (juce::Label::textColourId, Theme::textSecondary);
    c.box.addItemList (items, 1);
    c.box.setColour (juce::ComboBox::backgroundColourId, Theme::panel);
    c.box.setColour (juce::ComboBox::outlineColourId, Theme::panelEdge);
    c.box.setColour (juce::ComboBox::textColourId, Theme::textPrimary);
    addAndMakeVisible (c.label);
    addAndMakeVisible (c.box);
    c.attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (processor.getParameters(), paramId, c.box);
}

void PrismMeterEditor::updateScales()
{
    auto& params = processor.getParameters();
    const int vuChoice = choiceIndex (params, ParamIds::vuReference);
    const int ppmChoice = choiceIndex (params, ParamIds::ppmType);
    const int kChoice = choiceIndex (params, ParamIds::kScale);

    if (vuChoice == lastVuChoice && ppmChoice == lastPpmChoice && kChoice == lastKChoice)
        return;

    lastVuChoice = vuChoice;
    lastPpmChoice = ppmChoice;
    lastKChoice = kChoice;

    vuBank.setScale ({ -20.0f, 3.0f, 0.0f, 1.0f, { -20.0f, -10.0f, -7.0f, -5.0f, -3.0f, -1.0f, 0.0f, 1.0f, 3.0f }, "VU" });

    // PPM scales are shown in dB relative to alignment level (-18 dBFS); the
    // classic scale markings differ per type but the ballistics drive the bar.
    switch (ppmChoice)
    {
        case 2: // DIN: 0 dB = permitted max level (-9 dBFS here)
        case 3: // Nordic: 0 dB = alignment, TEST at -18 dBFS
            ppmBank.setScale ({ -42.0f, 9.0f, 0.0f, 6.0f, { -42.0f, -36.0f, -30.0f, -24.0f, -18.0f, -12.0f, -6.0f, 0.0f, 6.0f, 9.0f }, "dB" });
            break;
        default: // EBU / BBC: +/-12 around alignment
            ppmBank.setScale ({ -24.0f, 12.0f, 8.0f, 9.0f, { -24.0f, -12.0f, -8.0f, -4.0f, 0.0f, 4.0f, 8.0f, 12.0f }, "dB" });
            break;
    }
    ppmBank.setTitle ("PPM " + Choices::ppmTypes[ppmChoice]);

    kBank.setScale ({ -30.0f, Choices::kScaleValues[static_cast<size_t> (kChoice)], 0.0f, 4.0f,
                      { -30.0f, -20.0f, -10.0f, -6.0f, 0.0f, 4.0f, 8.0f, 12.0f }, "K" });
    kBank.setTitle (Choices::kScales[kChoice]);
}

void PrismMeterEditor::timerCallback()
{
    updateScales();

    auto& params = processor.getParameters();
    auto& s = processor.getMeterState();

    const float vuRef = Choices::vuReferenceValues[static_cast<size_t> (choiceIndex (params, ParamIds::vuReference))];
    const int ppmChoice = choiceIndex (params, ParamIds::ppmType);
    const float ppmRef = ppmChoice == 2 ? -9.0f : -18.0f;
    const float kRef = -Choices::kScaleValues[static_cast<size_t> (choiceIndex (params, ParamIds::kScale))];
    const int numChannels = s.numChannels.load();

    for (auto* bank : { &vuBank, &ppmBank, &kBank })
        bank->setNumBarsVisible (numChannels);

    float needlePeak = -150.0f, needleAverage = -150.0f;

    for (int ch = 0; ch < numChannels; ++ch)
    {
        const auto& c = s.channels[static_cast<size_t> (ch)];
        vuBank.setValue (ch, c.vu.load() - vuRef);
        ppmBank.setValue (ch, c.ppm.load() - ppmRef);
        kBank.setValue (ch, c.rms.load() - kRef);
        needlePeak = juce::jmax (needlePeak, c.needlePeak.load());
        needleAverage = juce::jmax (needleAverage, c.needleAverage.load());
    }

    vuBank.repaint();
    ppmBank.repaint();
    kBank.repaint();
    needleGauge.setValues (needlePeak, needleAverage);

    LoudnessPanel::Readings r;
    r.momentary = s.momentary.load();
    r.shortTerm = s.shortTerm.load();
    r.integrated = s.integrated.load();
    r.loudnessRange = s.loudnessRange.load();
    r.momentaryMax = s.momentaryMax.load();
    r.shortTermMax = s.shortTermMax.load();
    r.truePeakMax = s.truePeakMax.load();
    r.samplePeakMax = s.samplePeakMax.load();
    r.target = Choices::loudnessTargetValues[static_cast<size_t> (choiceIndex (params, ParamIds::loudnessTarget))];
    r.truePeakCeiling = Choices::truePeakCeilingValues[static_cast<size_t> (choiceIndex (params, ParamIds::truePeakCeiling))];
    loudnessPanel.setReadings (r);

    stereoPanel.setValues (s.correlation.load(), s.balanceDb.load(), s.width.load());

    const double sr = processor.getCurrentSampleRate();
    if (! juce::exactlyEqual (sr, lastSampleRate))
    {
        spectrum.prepare (sr);
        lastSampleRate = sr;
    }

    // Drain everything the audio thread produced since the last tick.
    for (;;)
    {
        const int n = processor.readSpectrumSamples (spectrumScratch.data(), static_cast<int> (spectrumScratch.size()));
        if (n <= 0)
            break;
        spectrum.pushSamples (spectrumScratch.data(), n);
    }
    spectrum.repaint();
}

void PrismMeterEditor::paint (juce::Graphics& g)
{
    g.fillAll (Theme::background);

    auto header = getLocalBounds().removeFromTop (44).reduced (14, 0);
    g.setColour (Theme::textPrimary);
    g.setFont (Theme::font (18.0f, true));
    g.drawText ("PRISM", header.removeFromLeft (70), juce::Justification::centredLeft);
    g.setColour (Theme::signal);
    g.setFont (Theme::font (18.0f));
    g.drawText ("meter", header.removeFromLeft (70), juce::Justification::centredLeft);
}

void PrismMeterEditor::resized()
{
    auto area = getLocalBounds();
    auto header = area.removeFromTop (44).reduced (14, 8);
    header.removeFromLeft (150);

    resetButton.setBounds (header.removeFromRight (70));
    header.removeFromRight (8);
    tiltButton.setBounds (header.removeFromRight (60));

    auto placeCombo = [&] (LabelledCombo& c, int labelWidth, int boxWidth)
    {
        c.label.setBounds (header.removeFromLeft (labelWidth));
        c.box.setBounds (header.removeFromLeft (boxWidth));
        header.removeFromLeft (8);
    };

    placeCombo (targetCombo, 52, 170);
    placeCombo (ceilingCombo, 58, 100);
    placeCombo (vuCombo, 50, 96);
    placeCombo (ppmCombo, 40, 84);
    placeCombo (kCombo, 24, 72);

    area = area.reduced (10);
    constexpr int gap = 10;

    auto left = area.removeFromLeft (juce::jmax (280, area.getWidth() / 4));
    area.removeFromLeft (gap);
    auto right = area.removeFromRight (juce::jmax (330, area.getWidth() * 3 / 10));
    area.removeFromRight (gap);

    loudnessPanel.setBounds (left.removeFromTop (juce::jmin (left.getHeight(), 330)));
    left.removeFromTop (gap);
    needleGauge.setBounds (left);

    stereoPanel.setBounds (area.removeFromBottom (86));
    area.removeFromBottom (gap);
    spectrum.setBounds (area);

    const int bankWidth = (right.getWidth() - 2 * gap) / 3;
    vuBank.setBounds (right.removeFromLeft (bankWidth));
    right.removeFromLeft (gap);
    ppmBank.setBounds (right.removeFromLeft (bankWidth));
    right.removeFromLeft (gap);
    kBank.setBounds (right);
}
