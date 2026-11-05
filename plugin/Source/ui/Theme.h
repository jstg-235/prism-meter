#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/// Prism Meter's own visual language: deep slate panels, a cool cyan signal
/// colour, warm amber for warnings and coral for overs.
namespace Theme
{
    inline const juce::Colour background { 0xff12161c };
    inline const juce::Colour panel { 0xff1a2029 };
    inline const juce::Colour panelEdge { 0xff2a3340 };
    inline const juce::Colour grid { 0xff26303c };
    inline const juce::Colour textPrimary { 0xffe6edf3 };
    inline const juce::Colour textSecondary { 0xff8b98a8 };
    inline const juce::Colour signal { 0xff3ec7d6 };
    inline const juce::Colour signalDim { 0x663ec7d6 };
    inline const juce::Colour secondary { 0xffb59cff };
    inline const juce::Colour warn { 0xfff2b54a };
    inline const juce::Colour over { 0xffff6b5e };
    inline const juce::Colour good { 0xff6fd18c };

    inline juce::Font font (float size, bool bold = false)
    {
        return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
    }

    inline void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title)
    {
        g.setColour (panel);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (panelEdge);
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);

        if (title.isNotEmpty())
        {
            g.setColour (textSecondary);
            g.setFont (font (11.0f, true));
            g.drawText (title.toUpperCase(), r.reduced (10.0f, 6.0f).removeFromTop (14.0f), juce::Justification::left);
        }
    }

    inline juce::String formatDb (float value, int decimals = 1)
    {
        if (! std::isfinite (value) || value <= -149.0f)
            return "-inf";
        return juce::String (value, decimals);
    }
}
