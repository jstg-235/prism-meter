#pragma once

#include <juce_core/juce_core.h>

#include <vector>

/// The meter tools a layout slot can show.
enum class MeterKind
{
    loudness,
    peakAverage,
    spectrum,
    phase,
    vu,
    ppm,
    kSystem,
    radar
};

namespace MeterKinds
{
    inline const std::vector<MeterKind>& all()
    {
        static const std::vector<MeterKind> kinds { MeterKind::loudness, MeterKind::peakAverage, MeterKind::spectrum,
                                                    MeterKind::phase, MeterKind::vu, MeterKind::ppm, MeterKind::kSystem,
                                                    MeterKind::radar };
        return kinds;
    }

    /// Stable id used in saved sessions. Never change an existing id.
    inline juce::String toId (MeterKind k)
    {
        switch (k)
        {
            case MeterKind::loudness:    return "loudness";
            case MeterKind::peakAverage: return "peakAverage";
            case MeterKind::spectrum:    return "spectrum";
            case MeterKind::phase:       return "phase";
            case MeterKind::vu:          return "vu";
            case MeterKind::ppm:         return "ppm";
            case MeterKind::kSystem:     return "kSystem";
            case MeterKind::radar:       return "radar";
        }
        return "loudness";
    }

    inline MeterKind fromId (const juce::String& id, MeterKind fallback)
    {
        for (auto k : all())
            if (toId (k) == id)
                return k;
        return fallback;
    }

    inline juce::String displayName (MeterKind k)
    {
        switch (k)
        {
            case MeterKind::loudness:    return "Loudness";
            case MeterKind::peakAverage: return "Peak + Average (dual needle)";
            case MeterKind::spectrum:    return "Spectrum + Spectrogram";
            case MeterKind::phase:       return "Phase / Vectorscope";
            case MeterKind::vu:          return "VU";
            case MeterKind::ppm:         return "PPM";
            case MeterKind::kSystem:     return "K-System";
            case MeterKind::radar:       return "Loudness Radar";
        }
        return {};
    }
}

/// One cell of a layout grid. Columns and rows are indices into the preset's
/// weight lists; spans let a slot cover several cells.
struct SlotSpec
{
    int col = 0, row = 0;
    int colSpan = 1, rowSpan = 1;
    MeterKind kind = MeterKind::loudness;
};

/// A layout tuned for one kind of output display. The window opens at
/// width x height (logical pixels, controls bar included) and the grid scales
/// with any later resize.
struct DisplayPreset
{
    juce::String id;            ///< stable, saved in sessions
    juce::String name;          ///< shown in the Display menu
    juce::String description;
    int width = 1280, height = 780;
    std::vector<float> columnWeights;
    std::vector<float> rowWeights;
    std::vector<SlotSpec> slots;
};

namespace DisplayPresets
{
    inline const std::vector<DisplayPreset>& all()
    {
        using K = MeterKind;

        static const std::vector<DisplayPreset> presets {
            { "console", "Console (full)", "Every meter on one screen, for a main studio display.",
              1280, 780,
              { 1.0f, 1.55f, 0.5f, 0.5f },
              { 0.50f, 0.12f, 0.08f, 0.30f },
              { { 0, 0, 1, 2, K::loudness }, { 0, 2, 1, 2, K::peakAverage },
                { 1, 0, 1, 3, K::spectrum }, { 1, 3, 1, 1, K::phase },
                { 2, 0, 1, 1, K::ppm }, { 3, 0, 1, 1, K::kSystem },
                { 2, 1, 2, 3, K::vu } } },

            { "wide12-stereo", "12\" widescreen: stereo Peak + Average", "One large stereo dual-needle meter, L and R side by side.",
              1280, 720,
              { 1.0f }, { 1.0f },
              { { 0, 0, 1, 1, K::peakAverage } } },

            { "wide12-vu", "12\" widescreen: stereo VU", "A pair of large analogue VU meters, L and R.",
              1280, 720,
              { 1.0f }, { 1.0f },
              { { 0, 0, 1, 1, K::vu } } },

            { "wide12-split", "12\" widescreen: two views", "Two meters side by side.",
              1280, 720,
              { 1.0f, 1.0f }, { 1.0f },
              { { 0, 0, 1, 1, K::peakAverage }, { 1, 0, 1, 1, K::loudness } } },

            { "classic14-quad", "14\" 4:3: four views (2 x 2)", "Four meters in a 2 x 2 grid for a traditional 4:3 monitor.",
              1024, 768,
              { 1.0f, 1.0f }, { 1.0f, 1.0f },
              { { 0, 0, 1, 1, K::loudness }, { 1, 0, 1, 1, K::spectrum },
                { 0, 1, 1, 1, K::peakAverage }, { 1, 1, 1, 1, K::phase } } },

            { "ultrawide-strip", "Ultrawide bar display: four views", "A row of four meters for a 1920 x 720 bar display.",
              1920, 720,
              { 0.55f, 0.55f, 1.0f, 1.6f }, { 1.0f },
              { { 0, 0, 1, 1, K::vu }, { 1, 0, 1, 1, K::ppm }, { 2, 0, 1, 1, K::loudness }, { 3, 0, 1, 1, K::spectrum } } },

            { "small7-loudness", "7\" small display: loudness", "Loudness only, for a small 800 x 480 screen.",
              800, 480,
              { 1.0f }, { 1.0f },
              { { 0, 0, 1, 1, K::loudness } } },
        };

        return presets;
    }

    inline const DisplayPreset& byId (const juce::String& id)
    {
        for (const auto& p : all())
            if (p.id == id)
                return p;
        return all().front();
    }

    inline int indexOf (const juce::String& id)
    {
        for (size_t i = 0; i < all().size(); ++i)
            if (all()[i].id == id)
                return static_cast<int> (i);
        return 0;
    }
}
