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
    for (auto* c : std::initializer_list<juce::Component*> { &loudnessPanel, &spectrum, &phasePanel, &needleGauge,
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
        loudnessPanel.clearHistory();
    };

    tiltButton.setToggleState (true, juce::dontSendNotification);
    tiltButton.setColour (juce::ToggleButton::tickColourId, Theme::spectrumAccent);
    tiltButton.setColour (juce::ToggleButton::textColourId, Theme::textSecondary);
    resetButton.setColour (juce::TextButton::buttonColourId, Theme::face);
    resetButton.setColour (juce::TextButton::textColourOffId, Theme::loudnessAccent);
    tiltButton.onClick = [this] { spectrum.setTiltEnabled (tiltButton.getToggleState()); };

    for (auto* bank : { &vuBank, &ppmBank, &kBank })
    {
        bank->setBarLabel (0, "L");
        bank->setBarLabel (1, "R");
    }

    setResizable (true, true);
    setResizeLimits (1180, 700, 2600, 1600);
    setSize (1280, 780);

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
    c.box.setColour (juce::ComboBox::backgroundColourId, Theme::face);
    c.box.setColour (juce::ComboBox::arrowColourId, Theme::textSecondary);
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

    phasePanel.setValues (s.correlation.load(), s.balanceDb.load(), s.width.load());

    // 10 Hz history for the loudness graph
    if (++tickCount % 3 == 0)
        loudnessPanel.pushHistory (r.shortTerm, r.momentary);

    // Vectorscope: draw only the newest samples, discard the backlog.
    {
        int n = 0;
        for (;;)
        {
            const int got = processor.readScopeSamples (scopeLeft.data(), scopeRight.data(), static_cast<int> (scopeLeft.size()));
            if (got <= 0)
                break;
            n = got;
        }
        if (n > 0)
            phasePanel.pushSamples (scopeLeft.data(), scopeRight.data(), juce::jmin (n, 1600));
        phasePanel.repaint();
    }

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
    // Wordmark: each letter of PRISM in one of the tool colours.
    const juce::Colour letters[] = { Theme::loudnessAccent, Theme::spectrumAccent, Theme::phaseAccent, Theme::vuAccent, Theme::ppmAccent };
    const juce::String word ("PRISM");
    g.setFont (Theme::font (20.0f, true));
    for (int i = 0; i < word.length(); ++i)
    {
        g.setColour (letters[i]);
        const auto letter = word.substring (i, i + 1);
        const int w = juce::roundToInt (juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), letter)) + 1;
        g.drawText (letter, header.removeFromLeft (w), juce::Justification::centredLeft);
    }
    header.removeFromLeft (6);
    g.setColour (Theme::textSecondary);
    g.setFont (Theme::font (18.0f));
    g.drawText ("meter", header.removeFromLeft (70), juce::Justification::centredLeft);

    g.setColour (Theme::panelEdge);
    g.drawHorizontalLine (43, 0.0f, static_cast<float> (getWidth()));
}

void PrismMeterEditor::resized()
{
    auto area = getLocalBounds();
    auto header = area.removeFromTop (44).reduced (14, 8);
    header.removeFromLeft (160);

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

    // Left: loudness (top), analogue peak + average gauge (bottom)
    auto left = area.removeFromLeft (juce::jmax (310, area.getWidth() / 4));
    area.removeFromLeft (gap);
    loudnessPanel.setBounds (left.removeFromTop (left.getHeight() * 62 / 100));
    left.removeFromTop (gap);
    needleGauge.setBounds (left);

    // Right: three segmented meter banks
    auto right = area.removeFromRight (juce::jmax (330, area.getWidth() * 3 / 10));
    area.removeFromRight (gap);
    const int bankWidth = (right.getWidth() - 2 * gap) / 3;
    vuBank.setBounds (right.removeFromLeft (bankWidth));
    right.removeFromLeft (gap);
    ppmBank.setBounds (right.removeFromLeft (bankWidth));
    right.removeFromLeft (gap);
    kBank.setBounds (right);

    // Centre: spectrum + spectrogram, phase / vectorscope below
    phasePanel.setBounds (area.removeFromBottom (juce::jmax (190, area.getHeight() * 30 / 100)));
    area.removeFromBottom (gap);
    spectrum.setBounds (area);
}
