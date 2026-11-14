// Renders the editor to a PNG after feeding it a synthetic programme signal.
// Developer tool for docs and visual checks:
//   PrismSnapshot <output.png>            current layout
//   PrismSnapshot --all-presets <dir>     one PNG per display preset
//   PrismSnapshot --layout <presetId> <meterId,meterId,...> <output.png> [seconds]
//                                         a preset with chosen meters in its slots,
//                                         after [seconds] of programme (default 12)

#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <juce_audio_utils/juce_audio_utils.h>

#include <random>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;

    const auto cwd = juce::File::getCurrentWorkingDirectory();
    const bool allPresets = argc > 1 && juce::String (argv[1]) == "--all-presets";
    const bool customLayout = argc > 4 && juce::String (argv[1]) == "--layout";
    const juce::File output = customLayout ? cwd.getChildFile (argv[4])
                            : allPresets ? cwd.getChildFile (argc > 2 ? argv[2] : "presets")
                                         : cwd.getChildFile (argc > 1 ? argv[1] : "prism-meter.png");

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

    renderSeconds (customLayout && argc > 5 ? juce::String (argv[5]).getDoubleValue() : 12.0);

    if (customLayout)
    {
        auto layout = processor.getParameters().state.getOrCreateChildWithName ("Layout", nullptr);
        layout.setProperty ("preset", juce::String (argv[2]), nullptr);
        layout.setProperty ("slots", juce::String (argv[3]), nullptr);
    }

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    auto* prism = dynamic_cast<PrismMeterEditor*> (editor.get());

    // Keep audio flowing while the editor's timer drains the analyser FIFOs.
    auto run = [&] (int ticks)
    {
        for (int tick = 0; tick < ticks; ++tick)
        {
            renderSeconds (1.0 / 30.0);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (34);
        }
    };

    auto save = [&] (const juce::File& file)
    {
        const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, editor->getWidth() > 1500 ? 1.5f : 2.0f);
        file.getParentDirectory().createDirectory();
        file.deleteFile();
        juce::FileOutputStream stream (file);
        juce::PNGImageFormat png;
        return stream.openedOk() && png.writeImageToStream (image, stream);
    };

    if (! allPresets)
    {
        run (450);
        return save (output) ? 0 : 1;
    }

    for (const auto& preset : DisplayPresets::all())
    {
        prism->applyPreset (preset.id);
        run (300);
        if (! save (output.getChildFile (preset.id + ".png")))
            return 1;
    }

    return 0;
}
