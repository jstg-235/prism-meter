#include "MeterViews.h"
#include "Theme.h"

#include <cmath>

//==============================================================================
BarMeterBank::BarMeterBank (juce::String t, juce::Colour a, int numBars)
    : title (std::move (t)),
      accent (a),
      values (static_cast<size_t> (numBars), -150.0f),
      holds (static_cast<size_t> (numBars), -150.0f),
      holdAge (static_cast<size_t> (numBars), 0),
      labels (static_cast<size_t> (numBars)),
      visibleBars (numBars)
{
}

void BarMeterBank::setScale (Scale newScale)
{
    scale = std::move (newScale);
    repaint();
}

void BarMeterBank::setTitle (juce::String newTitle)
{
    title = std::move (newTitle);
    repaint();
}

void BarMeterBank::setBarLabel (int index, juce::String label)
{
    labels[static_cast<size_t> (index)] = std::move (label);
}

void BarMeterBank::setNumBarsVisible (int n)
{
    visibleBars = juce::jlimit (1, static_cast<int> (values.size()), n);
}

void BarMeterBank::setValue (int index, float value)
{
    const auto i = static_cast<size_t> (index);
    values[i] = value;

    // Peak hold: ~1.5 s at 30 fps, then it follows the bar down.
    if (value >= holds[i] || ++holdAge[i] > 45)
    {
        holds[i] = value;
        holdAge[i] = 0;
    }
}

float BarMeterBank::toNorm (float value) const noexcept
{
    return (juce::jlimit (scale.minValue, scale.maxValue, value) - scale.minValue) / (scale.maxValue - scale.minValue);
}

juce::Colour BarMeterBank::zoneColour (float value) const noexcept
{
    if (value >= scale.overFrom)
        return Theme::levelRed;
    if (value >= scale.warnFrom)
        return Theme::levelYellow;
    return Theme::levelGreen;
}

void BarMeterBank::paint (juce::Graphics& g)
{
    auto area = Theme::drawPanel (g, getLocalBounds().toFloat(), title, accent).reduced (8.0f, 8.0f);

    auto readoutRow = area.removeFromTop (30.0f);
    area.removeFromTop (6.0f);
    auto labelRow = area.removeFromBottom (16.0f);
    auto scaleArea = area.removeFromLeft (26.0f);
    area.removeFromLeft (4.0f);

    Theme::drawFace (g, area);
    auto meterArea = area.reduced (4.0f);

    // Scale
    g.setFont (Theme::font (9.5f));
    for (float t : scale.ticks)
    {
        const float y = meterArea.getBottom() - toNorm (t) * meterArea.getHeight();
        g.setColour (Theme::textSecondary);
        g.drawText (juce::String (juce::roundToInt (t)), scaleArea.withY (y - 6.0f).withHeight (12.0f),
                    juce::Justification::centredRight);
        g.setColour (Theme::gridStrong);
        g.drawHorizontalLine (juce::roundToInt (y), area.getX(), area.getX() + 3.0f);
    }

    constexpr float segmentPitch = 5.0f; // 4 px lit + 1 px gap
    const int numSegments = juce::jmax (8, static_cast<int> (meterArea.getHeight() / segmentPitch));
    const float pitch = meterArea.getHeight() / static_cast<float> (numSegments);
    const float gap = 6.0f;
    const float barWidth = (meterArea.getWidth() - gap * static_cast<float> (visibleBars - 1)) / static_cast<float> (visibleBars);
    const float range = scale.maxValue - scale.minValue;

    // Reference line at the start of the warn zone (0 VU, alignment, K zero).
    {
        const float y = meterArea.getBottom() - toNorm (scale.warnFrom) * meterArea.getHeight();
        g.setColour (accent.withAlpha (0.55f));
        g.drawHorizontalLine (juce::roundToInt (y), area.getX(), area.getRight());
    }

    for (int b = 0; b < visibleBars; ++b)
    {
        const auto i = static_cast<size_t> (b);
        const float x = meterArea.getX() + static_cast<float> (b) * (barWidth + gap);
        const int litSegments = juce::roundToInt (toNorm (values[i]) * static_cast<float> (numSegments));
        const int holdSegment = juce::jlimit (0, numSegments - 1, juce::roundToInt (toNorm (holds[i]) * static_cast<float> (numSegments)) - 1);

        // Soft glow behind the lit column, like light bleeding through a diffuser.
        if (litSegments > 0)
        {
            const float top = meterArea.getBottom() - static_cast<float> (litSegments) * pitch;
            const float topValue = scale.minValue + static_cast<float> (litSegments) / static_cast<float> (numSegments) * range;
            g.setColour (zoneColour (topValue).withAlpha (0.10f));
            g.fillRoundedRectangle (juce::Rectangle<float>::leftTopRightBottom (x - 3.0f, top - 3.0f, x + barWidth + 3.0f, meterArea.getBottom() + 1.0f), 3.0f);
        }

        for (int s = 0; s < numSegments; ++s)
        {
            const float segValue = scale.minValue + (static_cast<float> (s) + 0.5f) / static_cast<float> (numSegments) * range;
            const auto colour = zoneColour (segValue);
            const bool lit = s < litSegments;
            const bool isHold = s == holdSegment && holds[i] > scale.minValue;
            const float y = meterArea.getBottom() - static_cast<float> (s + 1) * pitch;
            const auto seg = juce::Rectangle<float> (x, y + 0.5f, barWidth, pitch - 1.0f);

            if (lit || isHold)
            {
                // Lit LED: brighter centre, darker edges.
                const auto core = isHold ? colour.brighter (0.7f) : colour.brighter (0.15f);
                juce::ColourGradient grad (colour.darker (0.35f), seg.getX(), 0.0f, colour.darker (0.35f), seg.getRight(), 0.0f, false);
                grad.addColour (0.5, core);
                g.setGradientFill (grad);
                g.fillRoundedRectangle (seg, 1.0f);
            }
            else
            {
                g.setColour (colour.withAlpha (0.07f));
                g.fillRoundedRectangle (seg, 1.0f);
            }
        }
    }

    for (int b = 0; b < visibleBars; ++b)
    {
        const auto i = static_cast<size_t> (b);
        const float x = meterArea.getX() + static_cast<float> (b) * (barWidth + gap);

        // Per-bar peak-hold readout
        const auto box = readoutRow.withX (x).withWidth (barWidth);
        Theme::drawFace (g, box);
        g.setColour (holds[i] > scale.minValue ? zoneColour (holds[i]) : Theme::textSecondary);
        g.setFont (Theme::mono (barWidth < 44.0f ? 10.0f : 12.0f, true));
        g.drawFittedText (Theme::formatDb (holds[i]), box.toNearestInt(), juce::Justification::centred, 1, 0.8f);

        g.setColour (Theme::textSecondary);
        g.setFont (Theme::font (10.0f, true));
        g.drawText (labels[i], labelRow.withX (x).withWidth (barWidth), juce::Justification::centred);
    }

    g.setColour (accent.withAlpha (0.8f));
    g.setFont (Theme::font (9.5f, true));
    g.drawText (scale.unit, scaleArea.withY (labelRow.getY()).withHeight (16.0f), juce::Justification::centredRight);
}

//==============================================================================
void LoudnessPanel::setReadings (const Readings& r)
{
    readings = r;
    repaint();
}

void LoudnessPanel::pushHistory (float shortTermLufs, float momentaryLufs)
{
    shortHistory[static_cast<size_t> (historyWrite)] = shortTermLufs;
    momentaryHistory[static_cast<size_t> (historyWrite)] = momentaryLufs;
    historyWrite = (historyWrite + 1) % historyLength;
    historyCount = juce::jmin (historyCount + 1, historyLength);
}

void LoudnessPanel::clearHistory()
{
    historyCount = 0;
    historyWrite = 0;
}

void LoudnessPanel::paint (juce::Graphics& g)
{
    auto area = Theme::drawPanel (g, getLocalBounds().toFloat(), "Loudness  LUFS", Theme::loudnessAccent).reduced (8.0f);
    const auto& r = readings;

    // Readouts grow on large slots so the numbers read from across the room.
    const float k = juce::jlimit (1.0f, 2.4f, juce::jmin (area.getWidth() / 300.0f, area.getHeight() / 520.0f));

    auto colourForLoudness = [&] (float lufs)
    {
        if (! std::isfinite (lufs) || lufs <= -149.0f)
            return Theme::textSecondary;
        const float off = std::abs (lufs - r.target);
        return off <= 1.0f ? Theme::levelGreen : off <= 2.0f ? Theme::levelYellow : Theme::levelRed;
    };

    // Integrated, large
    auto hero = area.removeFromTop (58.0f * k);
    Theme::drawFace (g, hero);
    {
        auto inner = hero.reduced (8.0f, 4.0f);
        auto captionCol = inner.removeFromRight (inner.getWidth() * 0.36f);
        g.setColour (colourForLoudness (r.integrated));
        g.setFont (Theme::mono (36.0f * k, true));
        g.drawText (Theme::formatDb (r.integrated), inner, juce::Justification::centredLeft);
        g.setColour (Theme::loudnessAccent);
        g.setFont (Theme::font (10.0f * juce::jmin (k, 1.6f), true));
        g.drawText ("INTEGRATED", captionCol.removeFromTop (captionCol.getHeight() * 0.5f), juce::Justification::bottomRight);
        g.setColour (Theme::textSecondary);
        g.drawText ("TARGET " + juce::String (r.target, 0), captionCol, juce::Justification::topRight);
    }

    area.removeFromTop (6.0f);

    // 2 x 2 readouts
    auto grid = area.removeFromTop (84.0f * k);
    const float cellW = (grid.getWidth() - 6.0f) * 0.5f;
    auto rowA = grid.removeFromTop (39.0f * k);
    grid.removeFromTop (6.0f);
    auto rowB = grid;

    const bool tpOver = r.truePeakMax > r.truePeakCeiling;
    Theme::drawReadout (g, rowA.removeFromLeft (cellW), "Momentary", Theme::formatDb (r.momentary), colourForLoudness (r.momentary), 18.0f * k);
    Theme::drawReadout (g, rowA.withTrimmedLeft (6.0f), "Short-term", Theme::formatDb (r.shortTerm), colourForLoudness (r.shortTerm), 18.0f * k);
    Theme::drawReadout (g, rowB.removeFromLeft (cellW), "Range LU", juce::String (r.loudnessRange, 1), Theme::loudnessAccent, 18.0f * k);
    Theme::drawReadout (g, rowB.withTrimmedLeft (6.0f), "True peak dBTP", Theme::formatDb (r.truePeakMax),
                        tpOver ? Theme::levelRed : Theme::levelGreen, 18.0f * k);

    area.removeFromTop (6.0f);

    // Stats line at the bottom
    auto stats = area.removeFromBottom (16.0f);
    area.removeFromBottom (4.0f);
    {
        const bool hasI = std::isfinite (r.integrated) && r.integrated > -149.0f && r.truePeakMax > -149.0f;
        const juce::String text = "M max " + Theme::formatDb (r.momentaryMax) + "   S max " + Theme::formatDb (r.shortTermMax)
                                + "   PLR " + (hasI ? juce::String (r.truePeakMax - r.integrated, 1) : juce::String ("-"));
        g.setColour (Theme::textSecondary);
        g.setFont (Theme::font (10.5f));
        g.drawText (text, stats, juce::Justification::centredLeft);
    }

    // History graph
    Theme::drawFace (g, area);
    auto plot = area.reduced (4.0f).withTrimmedLeft (22.0f);
    constexpr float lo = -42.0f, hi = -2.0f;
    auto yFor = [&] (float lufs) { return plot.getBottom() - (juce::jlimit (lo, hi, lufs) - lo) / (hi - lo) * plot.getHeight(); };

    g.setFont (Theme::font (9.0f));
    for (float v = -40.0f; v <= -5.0f; v += 5.0f)
    {
        g.setColour (Theme::grid);
        g.drawHorizontalLine (juce::roundToInt (yFor (v)), plot.getX(), plot.getRight());
        g.setColour (Theme::textSecondary);
        g.drawText (juce::String (juce::roundToInt (v)), juce::Rectangle<float> (area.getX() + 2.0f, yFor (v) - 5.0f, 20.0f, 10.0f),
                    juce::Justification::centredLeft);
    }

    // Target band +/- 1 LU
    g.setColour (Theme::loudnessAccent.withAlpha (0.12f));
    g.fillRect (juce::Rectangle<float>::leftTopRightBottom (plot.getX(), yFor (r.target + 1.0f), plot.getRight(), yFor (r.target - 1.0f)));
    g.setColour (Theme::loudnessAccent.withAlpha (0.6f));
    g.drawHorizontalLine (juce::roundToInt (yFor (r.target)), plot.getX(), plot.getRight());

    if (historyCount > 1)
    {
        juce::Path shortPath, fillPath, momentaryPath;
        const float dx = plot.getWidth() / static_cast<float> (historyLength - 1);
        const int first = (historyWrite - historyCount + historyLength) % historyLength;
        const float x0 = plot.getRight() - static_cast<float> (historyCount - 1) * dx;

        for (int i = 0; i < historyCount; ++i)
        {
            const auto idx = static_cast<size_t> ((first + i) % historyLength);
            const float x = x0 + static_cast<float> (i) * dx;
            const float ys = yFor (std::isfinite (shortHistory[idx]) ? shortHistory[idx] : lo);
            const float ym = yFor (std::isfinite (momentaryHistory[idx]) ? momentaryHistory[idx] : lo);

            if (i == 0)
            {
                shortPath.startNewSubPath (x, ys);
                momentaryPath.startNewSubPath (x, ym);
                fillPath.startNewSubPath (x, plot.getBottom());
            }
            else
            {
                shortPath.lineTo (x, ys);
                momentaryPath.lineTo (x, ym);
            }
            fillPath.lineTo (x, ys);
        }

        fillPath.lineTo (plot.getRight(), plot.getBottom());
        fillPath.closeSubPath();

        g.setGradientFill (juce::ColourGradient (Theme::loudnessAccent.withAlpha (0.45f), 0.0f, plot.getY(),
                                                 Theme::loudnessAccent.withAlpha (0.03f), 0.0f, plot.getBottom(), false));
        g.fillPath (fillPath);
        g.setColour (Theme::needleAccent.withAlpha (0.35f));
        g.strokePath (momentaryPath, juce::PathStrokeType (1.0f));
        g.setColour (Theme::loudnessAccent);
        g.strokePath (shortPath, juce::PathStrokeType (1.8f));
    }

    g.setColour (Theme::textSecondary);
    g.setFont (Theme::font (9.0f));
    g.drawText ("short-term, last 60 s", plot.reduced (4.0f, 2.0f), juce::Justification::topRight);
}

//==============================================================================
void NeedleGauge::setValues (float peakDb, float averageDb)
{
    stereo = false;
    peak[0] = peak[1] = peakDb;
    average[0] = average[1] = averageDb;
    repaint();
}

void NeedleGauge::setStereoValues (float peakL, float averageL, float peakR, float averageR)
{
    stereo = true;
    peak[0] = peakL;
    average[0] = averageL;
    peak[1] = peakR;
    average[1] = averageR;
    repaint();
}

void NeedleGauge::paint (juce::Graphics& g)
{
    auto area = Theme::drawPanel (g, getLocalBounds().toFloat(), stereo ? "Peak + Average  stereo" : "Peak + Average",
                                  Theme::needleAccent).reduced (8.0f);

    // Two dials only when each still gets a sensible shape.
    const bool sideBySide = stereo && area.getWidth() >= area.getHeight() * 1.5f;

    if (sideBySide)
    {
        auto leftArea = area.removeFromLeft ((area.getWidth() - 8.0f) * 0.5f);
        area.removeFromLeft (8.0f);
        drawDial (g, leftArea, peak[0], average[0], "L");
        drawDial (g, area, peak[1], average[1], "R");
    }
    else
    {
        drawDial (g, area, juce::jmax (peak[0], peak[1]), juce::jmax (average[0], average[1]), stereo ? "L+R" : juce::String());
    }
}

void NeedleGauge::drawDial (juce::Graphics& g, juce::Rectangle<float> area, float peakDb, float averageDb,
                            const juce::String& label) const
{
    const float scale = juce::jlimit (1.0f, 2.2f, juce::jmin (area.getWidth() / 460.0f, area.getHeight() / 340.0f));
    const float legendHeight = 16.0f * scale;
    const float top = 34.0f * scale;      // room for the scale numbers above the arc
    const float bottom = 30.0f * scale;   // room below the pivot

    // Size the dial from the space, then shrink the face to fit it so a wide
    // slot does not leave an empty band under the pivot.
    const float radius = juce::jmax (20.0f, juce::jmin (area.getWidth() * 0.42f, area.getHeight() - legendHeight - 4.0f - top - bottom));
    area = area.withSizeKeepingCentre (area.getWidth(), juce::jmin (area.getHeight(), top + radius + bottom + 4.0f + legendHeight));

    auto legend = area.removeFromBottom (legendHeight);
    area.removeFromBottom (4.0f);

    // Ivory analogue face
    const auto faceColour = juce::Colour (0xfff1e6c8);
    const auto ink = juce::Colour (0xff1a1712);
    const auto red = juce::Colour (0xffc8281e);
    g.setGradientFill (juce::ColourGradient (faceColour.brighter (0.06f), area.getCentreX(), area.getY(),
                                             faceColour.darker (0.12f), area.getCentreX(), area.getBottom(), false));
    g.fillRoundedRectangle (area, 4.0f);
    g.setColour (juce::Colours::black);
    g.drawRoundedRectangle (area, 4.0f, 1.5f);

    const juce::Point<float> centre (area.getCentreX(), area.getY() + top + radius);

    constexpr float minDb = -40.0f, maxDb = 0.0f;
    constexpr float startAngle = -juce::MathConstants<float>::pi * 0.36f;
    constexpr float endAngle = juce::MathConstants<float>::pi * 0.36f;

    auto angleFor = [&] (float db)
    {
        const float norm = (juce::jlimit (minDb, maxDb, db) - minDb) / (maxDb - minDb);
        return startAngle + norm * (endAngle - startAngle);
    };
    auto pointAt = [&] (float angle, float rad)
    {
        return centre + juce::Point<float> (std::sin (angle) * rad, -std::cos (angle) * rad);
    };

    juce::Path arc;
    arc.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, startAngle, angleFor (-6.0f), true);
    g.setColour (ink);
    g.strokePath (arc, juce::PathStrokeType (1.5f * scale));

    juce::Path hot;
    hot.addCentredArc (centre.x, centre.y, radius - 3.0f * scale, radius - 3.0f * scale, 0.0f, angleFor (-6.0f), endAngle, true);
    g.setColour (red);
    g.strokePath (hot, juce::PathStrokeType (6.0f * scale));

    for (float t = minDb; t <= maxDb; t += 2.0f)
    {
        const bool major = juce::exactlyEqual (std::fmod (t, 10.0f), 0.0f) || t > -7.0f;
        const float a = angleFor (t);
        g.setColour (ink);
        g.drawLine ({ pointAt (a, radius), pointAt (a, radius + (major ? 9.0f : 5.0f) * scale) }, (major ? 1.4f : 0.8f) * scale);
    }

    g.setFont (Theme::font (10.5f * scale, true));
    for (float t : { -40.0f, -30.0f, -20.0f, -10.0f, -6.0f, -3.0f, 0.0f })
    {
        const auto p = pointAt (angleFor (t), radius + 18.0f * scale);
        g.setColour (t > -7.0f ? red : ink);
        g.drawText (juce::String (juce::roundToInt (t)), juce::Rectangle<float> (28.0f * scale, 12.0f * scale).withCentre (p),
                    juce::Justification::centred);
    }

    g.setColour (ink.withAlpha (0.55f));
    g.setFont (Theme::font (11.0f * scale, true));
    g.drawText ("dBFS", area.reduced (8.0f, 6.0f), juce::Justification::bottomRight);

    if (label.isNotEmpty())
    {
        g.setColour (ink.withAlpha (0.75f));
        g.setFont (Theme::font (16.0f * scale, true));
        g.drawText (label, area.reduced (10.0f, 6.0f), juce::Justification::bottomLeft);
    }

    auto needle = [&] (float db, juce::Colour c, float thickness)
    {
        const auto tip = pointAt (angleFor (db), radius + 4.0f * scale);
        g.setColour (juce::Colours::black.withAlpha (0.18f));
        g.drawLine ({ centre.translated (2.0f, 2.0f), tip.translated (2.0f, 2.0f) }, thickness);
        g.setColour (c);
        g.drawLine ({ centre, tip }, thickness);
    };

    needle (averageDb, ink, 2.4f * scale);
    needle (peakDb, juce::Colour (0xffd8322a), 1.8f * scale);

    g.setColour (ink);
    g.fillEllipse (juce::Rectangle<float> (12.0f * scale, 12.0f * scale).withCentre (centre));

    const float crest = (peakDb > -149.0f && averageDb > -149.0f) ? peakDb - averageDb : 0.0f;
    g.setFont (Theme::font (11.0f * scale, true));
    g.setColour (juce::Colour (0xffff5a4e));
    g.drawText ("PEAK " + Theme::formatDb (peakDb), legend, juce::Justification::left);
    g.setColour (Theme::needleAccent);
    g.drawText ("AVG " + Theme::formatDb (averageDb), legend, juce::Justification::centred);
    g.setColour (Theme::textSecondary);
    g.drawText ("CREST " + juce::String (crest, 1), legend, juce::Justification::right);
}

//==============================================================================
float AnalogVuMeter::needlePosition (float vu) noexcept
{
    // VU scales are linear in voltage: -20 VU is 0.1 of the 0 VU voltage.
    const float lo = 0.1f;
    const float hi = std::pow (10.0f, 3.0f / 20.0f);
    const float v = std::pow (10.0f, juce::jlimit (-60.0f, 6.0f, vu) / 20.0f);
    return juce::jlimit (-0.03f, 1.04f, (v - lo) / (hi - lo));
}

void AnalogVuMeter::setTitle (juce::String newTitle)
{
    if (newTitle != title)
    {
        title = std::move (newTitle);
        repaint();
    }
}

void AnalogVuMeter::setNumChannels (int n)
{
    numChannels = juce::jlimit (1, 2, n);
}

void AnalogVuMeter::setValue (int channel, float vu, bool peakOver)
{
    if (! juce::isPositiveAndBelow (channel, 2))
        return;

    const auto i = static_cast<size_t> (channel);
    values[i] = vu;
    // The lamp stays lit for half a second after an over, like a hardware lamp.
    lampFrames[i] = peakOver ? 15 : juce::jmax (0, lampFrames[i] - 1);
}

void AnalogVuMeter::paint (juce::Graphics& g)
{
    auto area = Theme::drawPanel (g, getLocalBounds().toFloat(), title, Theme::vuAccent).reduced (10.0f);

    constexpr float aspect = 1.45f; // width / height of one meter
    constexpr float gap = 12.0f;

    if (numChannels < 2)
    {
        drawMeter (g, area, 0, {});
        return;
    }

    // Side by side or stacked, whichever gives the bigger meters.
    const float sideH = juce::jmin (area.getHeight(), (area.getWidth() - gap) * 0.5f / aspect);
    const float stackH = juce::jmin ((area.getHeight() - gap) * 0.5f, area.getWidth() / aspect);

    if (sideH >= stackH)
    {
        auto left = area.removeFromLeft ((area.getWidth() - gap) * 0.5f);
        area.removeFromLeft (gap);
        drawMeter (g, left, 0, "L");
        drawMeter (g, area, 1, "R");
    }
    else
    {
        auto top = area.removeFromTop ((area.getHeight() - gap) * 0.5f);
        area.removeFromTop (gap);
        drawMeter (g, top, 0, "L");
        drawMeter (g, area, 1, "R");
    }
}

void AnalogVuMeter::drawMeter (juce::Graphics& g, juce::Rectangle<float> area, int channel, const juce::String& label) const
{
    constexpr float aspect = 1.45f;
    const float h = juce::jmin (area.getHeight(), area.getWidth() / aspect);
    auto bezel = area.withSizeKeepingCentre (h * aspect, h);
    const float s = juce::jlimit (0.55f, 2.4f, h / 260.0f); // type and line scale

    // --- Bezel: brushed dark metal with a bevel
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3d42), bezel.getX(), bezel.getY(),
                                             juce::Colour (0xff141518), bezel.getX(), bezel.getBottom(), false));
    g.fillRoundedRectangle (bezel, 8.0f * s);
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.drawRoundedRectangle (bezel.reduced (1.0f), 8.0f * s, 1.0f);
    g.setColour (juce::Colours::black);
    g.drawRoundedRectangle (bezel, 8.0f * s, 1.2f);

    const float lip = juce::jmax (6.0f, 11.0f * s);
    auto inner = bezel.reduced (lip);
    auto readoutStrip = inner.removeFromBottom (juce::jmax (14.0f, 22.0f * s));
    inner.removeFromBottom (lip * 0.5f);
    auto window = inner;

    // --- Backlit face: warm, brightest where the lamp sits behind the scale
    g.saveState();
    juce::Path windowPath;
    windowPath.addRoundedRectangle (window, 4.0f * s);
    g.reduceClipRegion (windowPath);

    const auto faceWarm = juce::Colour (0xfff7e3a6);
    const auto faceEdge = juce::Colour (0xffd9a94e);
    juce::ColourGradient glow (faceWarm.brighter (0.08f), window.getCentreX(), window.getY() + window.getHeight() * 0.55f,
                               faceEdge, window.getX(), window.getY(), true);
    g.setGradientFill (glow);
    g.fillRect (window);

    const auto ink = juce::Colour (0xff1c140a);
    const auto red = juce::Colour (0xffc3241b);

    const juce::Point<float> pivot (window.getCentreX(), window.getBottom() + window.getHeight() * 0.20f);
    const float radius = window.getHeight() * 0.92f;
    constexpr float span = juce::MathConstants<float>::pi * 0.25f; // +/- 45 degrees

    auto angleFor = [&] (float vu) { return -span + needlePosition (vu) * 2.0f * span; };
    auto pointAt = [&] (float angle, float r)
    {
        return pivot + juce::Point<float> (std::sin (angle) * r, -std::cos (angle) * r);
    };

    // Main scale arc, then the red band from 0 to +3 VU
    juce::Path arc;
    arc.addCentredArc (pivot.x, pivot.y, radius, radius, 0.0f, angleFor (-20.0f), angleFor (0.0f), true);
    g.setColour (ink);
    g.strokePath (arc, juce::PathStrokeType (1.6f * s));

    juce::Path redBand;
    const float bandR = radius + 4.0f * s;
    redBand.addCentredArc (pivot.x, pivot.y, bandR, bandR, 0.0f, angleFor (0.0f), angleFor (3.0f), true);
    g.setColour (red);
    g.strokePath (redBand, juce::PathStrokeType (8.0f * s, juce::PathStrokeType::mitered, juce::PathStrokeType::butt));

    // Ticks: majors are numbered, minors fill the gaps
    struct Mark { float vu; bool numbered; };
    static const Mark marks[] = {
        { -20.0f, true }, { -15.0f, false }, { -10.0f, true }, { -8.0f, false }, { -7.0f, true }, { -6.0f, false },
        { -5.0f, true }, { -4.0f, false }, { -3.0f, true }, { -2.0f, true }, { -1.0f, true }, { -0.5f, false },
        { 0.0f, true }, { 0.5f, false }, { 1.0f, true }, { 1.5f, false }, { 2.0f, true }, { 2.5f, false }, { 3.0f, true }
    };

    g.setFont (Theme::font (14.0f * s, true));
    for (const auto& m : marks)
    {
        const float a = angleFor (m.vu);
        const bool hot = m.vu > 0.0f;
        g.setColour (hot ? red : ink);
        const float len = (m.numbered ? 12.0f : 7.0f) * s;
        g.drawLine ({ pointAt (a, radius), pointAt (a, radius + len) }, (m.numbered ? 1.6f : 1.0f) * s);

        if (m.numbered)
        {
            const auto text = m.vu > 0.0f ? "+" + juce::String (juce::roundToInt (m.vu))
                                          : juce::String (juce::roundToInt (std::abs (m.vu)));
            g.drawText (text, juce::Rectangle<float> (30.0f * s, 16.0f * s).withCentre (pointAt (a, radius + 24.0f * s)),
                        juce::Justification::centred);
        }
    }

    // "-" and "+" end markers, out of the way of the numbers
    g.setFont (Theme::font (18.0f * s, true));
    g.setColour (ink);
    g.drawText ("-", juce::Rectangle<float> (20.0f * s, 20.0f * s).withCentre (pointAt (angleFor (-20.0f) - 0.10f, radius + 6.0f * s)),
                juce::Justification::centred);
    g.setColour (red);
    g.drawText ("+", juce::Rectangle<float> (20.0f * s, 20.0f * s).withCentre (pointAt (angleFor (3.0f) + 0.10f, radius + 6.0f * s)),
                juce::Justification::centred);

    // Secondary percent scale inside the arc (100 % = 0 VU)
    const float pctR = radius - 14.0f * s;
    juce::Path pctArc;
    pctArc.addCentredArc (pivot.x, pivot.y, pctR, pctR, 0.0f, angleFor (-20.0f), angleFor (0.0f), true);
    g.setColour (ink.withAlpha (0.55f));
    g.strokePath (pctArc, juce::PathStrokeType (0.9f * s));
    g.setFont (Theme::font (9.5f * s));
    for (int pct : { 20, 40, 60, 80, 100 })
    {
        const float vu = 20.0f * std::log10 (static_cast<float> (pct) / 100.0f);
        const float a = angleFor (vu);
        g.drawLine ({ pointAt (a, pctR), pointAt (a, pctR - 5.0f * s) }, 0.9f * s);
        g.drawText (juce::String (pct), juce::Rectangle<float> (26.0f * s, 12.0f * s).withCentre (pointAt (a, pctR - 13.0f * s)),
                    juce::Justification::centred);
    }

    // Legend
    g.setColour (ink.withAlpha (0.85f));
    g.setFont (Theme::font (26.0f * s, true));
    g.drawText ("VU", juce::Rectangle<float> (80.0f * s, 30.0f * s).withCentre ({ pivot.x, window.getBottom() - window.getHeight() * 0.30f }),
                juce::Justification::centred);

    g.setFont (Theme::font (8.5f * s, true));
    g.setColour (ink.withAlpha (0.5f));
    g.drawText ("NEW CONSTELLATION", window.reduced (8.0f * s, 6.0f * s), juce::Justification::bottomRight);

    if (label.isNotEmpty())
    {
        g.setColour (ink.withAlpha (0.8f));
        g.setFont (Theme::font (16.0f * s, true));
        g.drawText (label, window.reduced (9.0f * s, 5.0f * s), juce::Justification::bottomLeft);
    }

    // PEAK lamp, top left
    const auto idx = static_cast<size_t> (channel);
    const bool lit = lampFrames[idx] > 0;
    const auto lamp = juce::Rectangle<float> (10.0f * s, 10.0f * s).withPosition (window.getX() + 10.0f * s, window.getY() + 10.0f * s);
    if (lit)
    {
        g.setColour (juce::Colour (0xffff3020).withAlpha (0.35f));
        g.fillEllipse (lamp.expanded (5.0f * s));
    }
    g.setColour (lit ? juce::Colour (0xffff4a30) : juce::Colour (0xff5a1a12));
    g.fillEllipse (lamp);
    g.setColour (ink.withAlpha (0.6f));
    g.drawEllipse (lamp, 1.0f);
    g.setFont (Theme::font (9.0f * s, true));
    g.setColour (ink.withAlpha (0.75f));
    g.drawText ("PEAK", juce::Rectangle<float> (lamp.getRight() + 4.0f * s, lamp.getY() - 2.0f * s, 40.0f * s, lamp.getHeight() + 4.0f * s),
                juce::Justification::centredLeft);

    // Needle with a soft shadow on the face
    const float vu = values[idx];
    const float a = angleFor (vu);
    const auto tip = pointAt (a, radius + 10.0f * s);
    const auto shadowOffset = juce::Point<float> (2.5f * s, 3.0f * s);
    g.setColour (juce::Colours::black.withAlpha (0.22f));
    g.drawLine ({ pivot + shadowOffset, tip + shadowOffset }, 2.2f * s);
    g.setColour (juce::Colour (0xff111111));
    g.drawLine ({ pivot, tip }, 1.8f * s);

    // Mechanism cover across the bottom of the window hides the pivot
    const float coverR = window.getHeight() * 0.36f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2b2d31), pivot.x, pivot.y - coverR,
                                             juce::Colour (0xff121315), pivot.x, pivot.y, false));
    g.fillEllipse (juce::Rectangle<float> (coverR * 2.0f, coverR * 2.0f).withCentre (pivot));
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawEllipse (juce::Rectangle<float> (coverR * 2.0f, coverR * 2.0f).withCentre (pivot), 1.0f);

    // Glass: a soft top shadow and a diagonal glare
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.30f), window.getX(), window.getY(),
                                             juce::Colours::transparentBlack, window.getX(), window.getY() + 14.0f * s, false));
    g.fillRect (window);
    juce::Path glare;
    glare.startNewSubPath (window.getX(), window.getY());
    glare.lineTo (window.getX() + window.getWidth() * 0.62f, window.getY());
    glare.lineTo (window.getX() + window.getWidth() * 0.30f, window.getBottom());
    glare.lineTo (window.getX(), window.getBottom());
    glare.closeSubPath();
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.16f), window.getX(), window.getY(),
                                             juce::Colours::white.withAlpha (0.0f), window.getCentreX(), window.getBottom(), false));
    g.fillPath (glare);

    g.restoreState();

    // Numeric reading in the bezel under the window
    g.setColour (Theme::face);
    g.fillRoundedRectangle (readoutStrip.withSizeKeepingCentre (juce::jmin (readoutStrip.getWidth(), 120.0f * s), readoutStrip.getHeight()), 3.0f);
    g.setFont (Theme::mono (juce::jmax (10.0f, 14.0f * s), true));
    g.setColour (vu > 0.0f ? Theme::levelRed : Theme::vuAccent);
    g.drawText ((vu <= -40.0f ? juce::String ("-inf") : juce::String (vu, 1)) + " VU", readoutStrip, juce::Justification::centred);
}

//==============================================================================
PhasePanel::PhasePanel()
{
    trace = juce::Image (juce::Image::RGB, 256, 256, true);
}

void PhasePanel::resized()
{
    trace.clear (trace.getBounds(), juce::Colours::black);
}

void PhasePanel::setValues (float c, float b, float w)
{
    correlation = c;
    balance = b;
    width = w;
}

void PhasePanel::fadeTrace()
{
    juce::Graphics g (trace);
    g.setColour (juce::Colours::black.withAlpha (0.22f));
    g.fillAll();
}

void PhasePanel::pushSamples (const float* left, const float* right, int numSamples)
{
    fadeTrace();

    float peak = 0.0f;
    for (int i = 0; i < numSamples; ++i)
        peak = juce::jmax (peak, std::abs (left[i]), std::abs (right[i]));

    // Auto-gain so quiet material still fills the scope; fast up, slow down.
    const float wanted = peak > 1.0e-4f ? 0.85f / (peak * juce::MathConstants<float>::sqrt2) : autoGain;
    autoGain = wanted < autoGain ? wanted : autoGain + 0.05f * (wanted - autoGain);
    autoGain = juce::jlimit (0.5f, 200.0f, autoGain);

    juce::Graphics g (trace);
    const float half = static_cast<float> (trace.getWidth()) * 0.5f;
    const float k = half * autoGain / juce::MathConstants<float>::sqrt2;

    for (int i = 0; i < numSamples; ++i)
    {
        const float x = half + (right[i] - left[i]) * k;
        const float y = half - (left[i] + right[i]) * k;
        g.setColour (Theme::scopeTrace.withAlpha (0.35f));
        g.fillRect (x, y, 1.6f, 1.6f);
    }
}

void PhasePanel::paint (juce::Graphics& g)
{
    auto area = Theme::drawPanel (g, getLocalBounds().toFloat(), "Phase  Vectorscope", Theme::phaseAccent).reduced (8.0f);

    // Square scope on the left
    const float side = juce::jmin (area.getHeight(), area.getWidth() * 0.45f);
    auto scopeArea = area.removeFromLeft (side).withSizeKeepingCentre (side, side);
    area.removeFromLeft (10.0f);

    Theme::drawFace (g, scopeArea);
    g.drawImage (trace, scopeArea.reduced (2.0f), juce::RectanglePlacement::stretchToFit);

    // Graticule: L and R diagonals, M vertical, S horizontal
    const auto c = scopeArea.getCentre();
    const float r = side * 0.5f - 4.0f;
    g.setColour (Theme::phaseAccent.withAlpha (0.25f));
    g.drawLine (c.x, scopeArea.getY() + 4.0f, c.x, scopeArea.getBottom() - 4.0f, 1.0f);
    g.drawLine (scopeArea.getX() + 4.0f, c.y, scopeArea.getRight() - 4.0f, c.y, 1.0f);
    g.setColour (Theme::phaseAccent.withAlpha (0.15f));
    g.drawLine (c.x - r * 0.7f, c.y - r * 0.7f, c.x + r * 0.7f, c.y + r * 0.7f, 1.0f);
    g.drawLine (c.x + r * 0.7f, c.y - r * 0.7f, c.x - r * 0.7f, c.y + r * 0.7f, 1.0f);
    g.drawEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c), 1.0f);

    g.setColour (Theme::phaseAccent.withAlpha (0.8f));
    g.setFont (Theme::font (9.5f, true));
    g.drawText ("M", juce::Rectangle<float> (12.0f, 12.0f).withCentre ({ c.x + 9.0f, scopeArea.getY() + 10.0f }), juce::Justification::centred);
    g.drawText ("L", juce::Rectangle<float> (12.0f, 12.0f).withCentre ({ c.x - r * 0.7f - 4.0f, c.y - r * 0.7f - 4.0f }), juce::Justification::centred);
    g.drawText ("R", juce::Rectangle<float> (12.0f, 12.0f).withCentre ({ c.x + r * 0.7f + 4.0f, c.y - r * 0.7f - 4.0f }), juce::Justification::centred);
    g.drawText ("S", juce::Rectangle<float> (12.0f, 12.0f).withCentre ({ scopeArea.getRight() - 9.0f, c.y - 8.0f }), juce::Justification::centred);

    // Correlation bar, multi-coloured: red below 0, yellow 0..0.3, green above
    auto corrBox = area.removeFromTop (44.0f);
    Theme::drawFace (g, corrBox);
    auto inner = corrBox.reduced (8.0f, 5.0f);
    g.setColour (Theme::textSecondary);
    g.setFont (Theme::font (9.5f, true));
    g.drawText ("CORRELATION", inner.removeFromTop (12.0f), juce::Justification::left);
    auto bar = inner.removeFromTop (12.0f);

    juce::ColourGradient grad (Theme::levelRed, bar.getX(), 0.0f, Theme::levelGreen, bar.getRight(), 0.0f, false);
    grad.addColour (0.5, Theme::levelYellow);
    g.setGradientFill (grad);
    g.setOpacity (0.18f);
    g.fillRect (bar);
    g.setOpacity (1.0f);

    const float cx = bar.getCentreX();
    const float x = cx + juce::jlimit (-1.0f, 1.0f, correlation) * bar.getWidth() * 0.5f;
    const auto barColour = correlation < 0.0f ? Theme::levelRed : correlation < 0.3f ? Theme::levelYellow : Theme::levelGreen;
    g.setColour (barColour);
    g.fillRect (juce::Rectangle<float>::leftTopRightBottom (juce::jmin (cx, x), bar.getY(), juce::jmax (cx, x), bar.getBottom()));
    g.setColour (Theme::textPrimary);
    g.fillRect (juce::Rectangle<float> (2.0f, bar.getHeight() + 4.0f).withCentre ({ x, bar.getCentreY() }));
    g.setColour (Theme::textSecondary);
    g.setFont (Theme::font (9.0f));
    auto ticks = inner;
    g.drawText ("-1", ticks, juce::Justification::left);
    g.drawText ("0", ticks, juce::Justification::centred);
    g.drawText ("+1", ticks, juce::Justification::right);

    area.removeFromTop (6.0f);
    const float cellW = (area.getWidth() - 12.0f) / 3.0f;
    auto row = area.removeFromTop (juce::jmin (40.0f, area.getHeight()));
    Theme::drawReadout (g, row.removeFromLeft (cellW), "Corr", juce::String (correlation, 2), barColour, 15.0f);
    row.removeFromLeft (6.0f);
    Theme::drawReadout (g, row.removeFromLeft (cellW), "Bal dB", juce::String (balance, 1), Theme::phaseAccent, 15.0f);
    row.removeFromLeft (6.0f);
    Theme::drawReadout (g, row, "Width", juce::String (width, 2), Theme::phaseAccent, 15.0f);
}

//==============================================================================
SpectrumView::SpectrumView()
{
    prepare (48000.0);
}

void SpectrumView::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    analyzer.prepare (fftOrder, sampleRate);
    analyzer.setAveraging (0.75f);
    ring.assign (static_cast<size_t> (analyzer.getFftSize()), 0.0f);
    frame.assign (ring.size(), 0.0f);
    ringWrite = 0;
    samplesSinceFrame = 0;
    spectrogram = juce::Image (juce::Image::RGB, 480, spectrogramRows, true);
}

void SpectrumView::resized() {}

void SpectrumView::pushSamples (const float* samples, int numSamples)
{
    const int size = static_cast<int> (ring.size());

    for (int i = 0; i < numSamples; ++i)
    {
        ring[static_cast<size_t> (ringWrite)] = samples[i];
        ringWrite = (ringWrite + 1) % size;

        if (++samplesSinceFrame >= hopSize)
        {
            samplesSinceFrame = 0;
            for (int k = 0; k < size; ++k)
                frame[static_cast<size_t> (k)] = ring[static_cast<size_t> ((ringWrite + k) % size)];
            analyzer.processFrame (frame.data());
            addSpectrogramColumn();
        }
    }
}

float SpectrumView::tiltAt (float hz) const noexcept
{
    return tilt ? 4.5f * std::log2 (hz / 1000.0f) : 0.0f;
}

float SpectrumView::levelBetween (float hzLo, float hzHi, bool peak) const noexcept
{
    const auto& data = peak ? analyzer.getPeakHoldDb() : analyzer.getMagnitudesDb();
    const int numBins = analyzer.getNumBins();
    const float binHz = static_cast<float> (sampleRate) / static_cast<float> (analyzer.getFftSize());
    const int lo = juce::jlimit (1, numBins - 1, static_cast<int> (hzLo / binHz));
    const int hi = juce::jlimit (lo, numBins - 1, static_cast<int> (hzHi / binHz));

    float level = -200.0f;
    for (int b = lo; b <= hi; ++b)
        level = juce::jmax (level, data[static_cast<size_t> (b)]);
    return level;
}

void SpectrumView::addSpectrogramColumn()
{
    const int w = spectrogram.getWidth();
    const int h = spectrogram.getHeight();
    spectrogram.moveImageSection (0, 0, 1, 0, w - 1, h);

    for (int row = 0; row < h; ++row)
    {
        // Row 0 is 20 kHz, the last row 20 Hz.
        const float pos = 1.0f - (static_cast<float> (row) + 0.5f) / static_cast<float> (h);
        const float hzLo = 20.0f * std::pow (1000.0f, pos - 0.5f / static_cast<float> (h));
        const float hzHi = 20.0f * std::pow (1000.0f, pos + 0.5f / static_cast<float> (h));
        const float db = levelBetween (hzLo, hzHi, false) + tiltAt (0.5f * (hzLo + hzHi));
        spectrogram.setPixelAt (w - 1, row, Theme::heat ((db + 90.0f) / 84.0f));
    }
}

void SpectrumView::paint (juce::Graphics& g)
{
    auto area = Theme::drawPanel (g, getLocalBounds().toFloat(), tilt ? "Spectrum  4.5 dB/oct tilt" : "Spectrum",
                                  Theme::spectrumAccent).reduced (8.0f);

    auto sgArea = area.removeFromBottom (juce::jmax (60.0f, area.getHeight() * 0.30f));
    area.removeFromBottom (6.0f);

    // ---- Spectrum
    Theme::drawFace (g, area);
    auto plot = area.reduced (4.0f).withTrimmedLeft (24.0f).withTrimmedBottom (14.0f);
    constexpr float minDb = -90.0f, maxDb = 6.0f;
    auto dbToY = [&] (float db) { return plot.getBottom() - (juce::jlimit (minDb, maxDb, db) - minDb) / (maxDb - minDb) * plot.getHeight(); };
    auto hzToX = [&] (float hz) { return plot.getX() + plot.getWidth() * std::log (hz / 20.0f) / std::log (1000.0f); };

    g.setFont (Theme::font (9.5f));
    for (float db = -84.0f; db <= 0.0f; db += 12.0f)
    {
        g.setColour (Theme::grid);
        g.drawHorizontalLine (juce::roundToInt (dbToY (db)), plot.getX(), plot.getRight());
        g.setColour (Theme::textSecondary);
        g.drawText (juce::String (juce::roundToInt (db)), juce::Rectangle<float> (area.getX() + 2.0f, dbToY (db) - 6.0f, 24.0f, 12.0f),
                    juce::Justification::centredLeft);
    }

    for (float hz : { 30.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f })
    {
        const float x = hzToX (hz);
        g.setColour (Theme::grid);
        g.drawVerticalLine (juce::roundToInt (x), plot.getY(), plot.getBottom());
        g.setColour (Theme::frequencyColour (hz).withAlpha (0.9f));
        const auto label = hz >= 1000.0f ? juce::String (juce::roundToInt (hz / 1000.0f)) + "k" : juce::String (juce::roundToInt (hz));
        g.drawText (label, juce::Rectangle<float> (x - 18.0f, plot.getBottom() + 2.0f, 36.0f, 12.0f), juce::Justification::centred);
    }

    juce::Path line, fill, peakLine;
    const int width = juce::roundToInt (plot.getWidth());
    for (int px = 0; px <= width; px += 2)
    {
        const float hzLo = 20.0f * std::pow (1000.0f, static_cast<float> (px) / static_cast<float> (width));
        const float hzHi = 20.0f * std::pow (1000.0f, static_cast<float> (px + 2) / static_cast<float> (width));
        const float t = tiltAt (hzLo);
        const float x = plot.getX() + static_cast<float> (px);
        const float y = dbToY (levelBetween (hzLo, hzHi, false) + t);
        const float py = dbToY (levelBetween (hzLo, hzHi, true) + t);

        if (px == 0)
        {
            line.startNewSubPath (x, y);
            fill.startNewSubPath (x, plot.getBottom());
            fill.lineTo (x, y);
            peakLine.startNewSubPath (x, py);
        }
        else
        {
            line.lineTo (x, y);
            fill.lineTo (x, y);
            peakLine.lineTo (x, py);
        }
    }
    fill.lineTo (plot.getRight(), plot.getBottom());
    fill.closeSubPath();

    juce::ColourGradient rainbow (Theme::frequencyColour (20.0f), plot.getX(), 0.0f,
                                  Theme::frequencyColour (20000.0f), plot.getRight(), 0.0f, false);
    for (float hz : { 60.0f, 200.0f, 600.0f, 2000.0f, 6000.0f })
        rainbow.addColour ((hzToX (hz) - plot.getX()) / plot.getWidth(), Theme::frequencyColour (hz));

    g.setGradientFill (rainbow);
    g.setOpacity (0.35f);
    g.fillPath (fill);
    g.setOpacity (1.0f);
    g.setGradientFill (rainbow);
    g.strokePath (line, juce::PathStrokeType (1.8f));
    g.setColour (Theme::textPrimary.withAlpha (0.45f));
    g.strokePath (peakLine, juce::PathStrokeType (1.0f));

    // ---- Spectrogram
    Theme::drawFace (g, sgArea);
    auto sgPlot = sgArea.reduced (4.0f).withTrimmedLeft (24.0f);
    g.drawImage (spectrogram, sgPlot, juce::RectanglePlacement::stretchToFit);
    g.setFont (Theme::font (9.0f));
    for (float hz : { 100.0f, 1000.0f, 10000.0f })
    {
        const float y = sgPlot.getBottom() - sgPlot.getHeight() * std::log (hz / 20.0f) / std::log (1000.0f);
        g.setColour (Theme::frequencyColour (hz));
        g.drawText (hz >= 1000.0f ? juce::String (juce::roundToInt (hz / 1000.0f)) + "k" : juce::String (juce::roundToInt (hz)),
                    juce::Rectangle<float> (sgArea.getX() + 2.0f, y - 5.0f, 22.0f, 10.0f), juce::Justification::centredLeft);
    }
    g.setColour (Theme::textSecondary);
    g.drawText ("time", sgPlot.reduced (4.0f, 2.0f), juce::Justification::bottomRight);
}
