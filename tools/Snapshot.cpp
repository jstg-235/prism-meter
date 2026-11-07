// Renders the editor to a PNG after feeding it a synthetic programme signal.
// Developer tool for docs and visual checks: PrismSnapshot <output.png>

#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <juce_audio_utils/juce_audio_utils.h>

#include <random>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;

    const juce::File output (argc > 1 ? juce::File::getCurrentWorkingDirectory().getChildFile (argv[1])
                                      : juce::File::getCurrentWorkingDirectory().getChildFile ("prism-meter.png"));

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;

    PrismMeterProcessor processor;
    processor.setPlayConfigDetails (2, 2, sampleRate, blockSize);
    processor.prepareToPlay (sampleRate, blockSize);

    // Pink-ish noise (one-pole filtered white) plus a 60 Hz "kick" every 500 ms,
    // slightly decorrelated between channels.
    std::mt19937 rng (42);
    std::normal_distribution<float> noise (0.0f, 1.0f);
    float lpL = 0.0f, lpR = 0.0f;
    long sampleIndex = 0;
    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::MidiBuffer midi;

    auto renderSeconds = [&] (double seconds)
    {
        const int blocks = static_cast<int> (seconds * sampleRate / blockSize);
        for (int b = 0; b < blocks; ++b)
        {
            for (int i = 0; i < blockSize; ++i, ++sampleIndex)
            {
                const float common = noise (rng);
                lpL += 0.05f * (0.7f * common + 0.3f * noise (rng) - lpL);
                lpR += 0.05f * (0.7f * common + 0.3f * noise (rng) - lpR);

                const double t = static_cast<double> (sampleIndex % 24000) / sampleRate;
                const auto kick = static_cast<float> (0.5 * std::exp (-t * 18.0) * std::sin (2.0 * juce::MathConstants<double>::pi * 60.0 * t));

                // Slow swell so the loudness history has some movement.
                const auto swell = static_cast<float> (0.75 + 0.25 * std::sin (2.0 * juce::MathConstants<double>::pi * static_cast<double> (sampleIndex) / (sampleRate * 7.0)));
                buffer.setSample (0, i, swell * (0.55f * lpL + kick));
                buffer.setSample (1, i, swell * (0.55f * lpR + kick));
            }
            processor.processBlock (buffer, midi);
        }
    };

    renderSeconds (12.0);

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());

    // Keep audio flowing while the editor's timer drains the spectrum FIFO.
    for (int tick = 0; tick < 450; ++tick)
    {
        renderSeconds (1.0 / 30.0);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (34);
    }

    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
    juce::FileOutputStream stream (output);
    if (! stream.openedOk())
        return 1;

    stream.setPosition (0);
    stream.truncate();
    juce::PNGImageFormat png;
    return png.writeImageToStream (image, stream) ? 0 : 1;
}
