#include "MeterViews.h"
#include "Theme.h"

#include <cmath>

//==============================================================================
BarMeterBank::BarMeterBank (juce::String t, int numBars)
    : title (std::move (t)),
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

    // Peak-hold marker: holds for ~1.5 s at 30 fps, then follows the bar down.
    if (value >= holds[i] || ++holdAge[i] > 45)
    {
        holds[i] = value;
        holdAge[i] = 0;
    }
}

float BarMeterBank::toY (float value, juce::Rectangle<float> area) const noexcept
{
    const float v = juce::jlimit (scale.minValue, scale.maxValue, value);
    const float norm = (v - scale.minValue) / (scale.maxValue - scale.minValue);
    return area.getBottom() - norm * area.getHeight();
}

void BarMeterBank::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    Theme::drawPanel (g, bounds, title);

    auto area = bounds.reduced (10.0f).withTrimmedTop (18.0f);
    auto labelRow = area.removeFromBottom (16.0f);
    auto scaleArea = area.removeFromLeft (30.0f);
    area.removeFromLeft (4.0f);

    // Scale
    g.setFont (Theme::font (10.0f));
    for (float t : scale.ticks)
    {
        const float y = toY (t, area);
        g.setColour (Theme::grid);
        g.drawHorizontalLine (juce::roundToInt (y), area.getX(), area.getRight());
        g.setColour (Theme::textSecondary);
        g.drawText (juce::String (juce::roundToInt (t)), scaleArea.withY (y - 6.0f).withHeight (12.0f),
                    juce::Justification::centredRight);
    }

    const float gap = 6.0f;
    const float barWidth = (area.getWidth() - gap * static_cast<float> (visibleBars - 1)) / static_cast<float> (visibleBars);

    for (int b = 0; b < visibleBars; ++b)
    {
        const auto i = static_cast<size_t> (b);
        const auto bar = juce::Rectangle<float> (area.getX() + static_cast<float> (b) * (barWidth + gap), area.getY(),
                                                 barWidth, area.getHeight());

        g.setColour (Theme::background);
        g.fillRect (bar);

        const float valueY = toY (values[i], bar);
        const float warnY = toY (scale.warnFrom, bar);
        const float overY = toY (scale.overFrom, bar);

        // Draw in three colour zones so the colour always reflects the scale position.
        auto drawZone = [&] (float from, float to, juce::Colour c)
        {
            const float top = juce::jmax (valueY, to);
            if (top < from)
            {
                g.setColour (c);
                g.fillRect (bar.withTop (top).withBottom (from));
            }
        };

        drawZone (bar.getBottom(), warnY, Theme::signal);
        drawZone (warnY, overY, Theme::warn);
        drawZone (overY, bar.getY(), Theme::over);

        const float holdY = toY (holds[i], bar);
        g.setColour (Theme::textPrimary);
        g.fillRect (bar.withY (holdY - 1.0f).withHeight (2.0f));

        g.setColour (Theme::textSecondary);
        g.setFont (Theme::font (10.0f));
        g.drawText (labels[i], labelRow.withX (bar.getX()).withWidth (barWidth), juce::Justification::centred);
    }

    // Readout of the loudest bar
    float loudest = -150.0f;
    for (int b = 0; b < visibleBars; ++b)
        loudest = juce::jmax (loudest, values[static_cast<size_t> (b)]);

    g.setColour (Theme::textPrimary);
    g.setFont (Theme::font (11.0f, true));
    g.drawText (Theme::formatDb (loudest) + " " + scale.unit, bounds.reduced (10.0f, 6.0f).removeFromTop (14.0f),
                juce::Justification::right);
}

//==============================================================================
void LoudnessPanel::setReadings (const Readings& r)
{
    readings = r;
    repaint();
}

void LoudnessPanel::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    Theme::drawPanel (g, bounds, "Loudness");

    auto area = bounds.reduced (12.0f).withTrimmedTop (18.0f);
    const auto& r = readings;

    // Integrated, large, coloured by distance from target.
    auto hero = area.removeFromTop (64.0f);
    const bool hasIntegrated = std::isfinite (r.integrated) && r.integrated > -149.0f;
    const float offTarget = hasIntegrated ? r.integrated - r.target : 0.0f;
    const juce::Colour heroColour = ! hasIntegrated ? Theme::textSecondary
                                  : std::abs (offTarget) <= 1.0f ? Theme::good
                                  : std::abs (offTarget) <= 2.0f ? Theme::warn
                                                                 : Theme::over;
    g.setColour (heroColour);
    g.setFont (Theme::font (40.0f, true));
    g.drawText (Theme::formatDb (r.integrated), hero.removeFromLeft (hero.getWidth() * 0.62f), juce::Justification::left);
    g.setColour (Theme::textSecondary);
    g.setFont (Theme::font (12.0f));
    g.drawText ("LUFS integrated", hero.removeFromTop (32.0f), juce::Justification::bottomLeft);
    g.drawText ("target " + juce::String (r.target, 0), hero, juce::Justification::topLeft);

    area.removeFromTop (8.0f);

    // Momentary and short-term bars, horizontal, -40..0 LUFS, target line.
    auto drawBar = [&] (const juce::String& name, float value, float maxValue)
    {
        auto row = area.removeFromTop (34.0f);
        auto label = row.removeFromTop (14.0f);
        g.setColour (Theme::textSecondary);
        g.setFont (Theme::font (11.0f));
        g.drawText (name, label, juce::Justification::left);
        g.setColour (Theme::textPrimary);
        g.drawText (Theme::formatDb (value) + "   max " + Theme::formatDb (maxValue), label, juce::Justification::right);

        auto bar = row.withTrimmedBottom (6.0f);
        g.setColour (Theme::background);
        g.fillRoundedRectangle (bar, 3.0f);

        auto toX = [&] (float lufs)
        {
            const float norm = (juce::jlimit (-40.0f, 0.0f, lufs) + 40.0f) / 40.0f;
            return bar.getX() + norm * bar.getWidth();
        };

        if (std::isfinite (value))
        {
            g.setColour (value > r.target + 1.0f ? Theme::warn : Theme::signal);
            g.fillRoundedRectangle (bar.withRight (toX (value)), 3.0f);
        }

        g.setColour (Theme::textPrimary);
        const float tx = toX (r.target);
        g.drawLine (tx, bar.getY() - 2.0f, tx, bar.getBottom() + 2.0f, 1.5f);
    };

    drawBar ("Momentary", r.momentary, r.momentaryMax);
    drawBar ("Short-term", r.shortTerm, r.shortTermMax);

    area.removeFromTop (6.0f);

    // Stat grid
    const float plr = (hasIntegrated && r.truePeakMax > -149.0f) ? r.truePeakMax - r.integrated : 0.0f;
    const bool tpOver = r.truePeakMax > r.truePeakCeiling;

    struct Stat { juce::String name, value; juce::Colour colour; };
    const Stat stats[] = {
        { "LRA", juce::String (r.loudnessRange, 1) + " LU", Theme::textPrimary },
        { "True peak max", Theme::formatDb (r.truePeakMax) + " dBTP", tpOver ? Theme::over : Theme::textPrimary },
        { "Sample peak max", Theme::formatDb (r.samplePeakMax) + " dBFS", Theme::textPrimary },
        { "PLR", hasIntegrated ? juce::String (plr, 1) + " LU" : juce::String ("-"), Theme::textPrimary },
    };

    for (const auto& s : stats)
    {
        auto row = area.removeFromTop (20.0f);
        g.setColour (Theme::textSecondary);
        g.setFont (Theme::font (12.0f));
        g.drawText (s.name, row, juce::Justification::left);
        g.setColour (s.colour);
        g.setFont (Theme::font (12.0f, true));
        g.drawText (s.value, row, juce::Justification::right);
    }
}

//==============================================================================
void NeedleGauge::setValues (float peakDb, float averageDb)
{
    peak = peakDb;
    average = averageDb;
    repaint();
}

void NeedleGauge::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    Theme::drawPanel (g, bounds, "Peak + Average");

    auto area = bounds.reduced (12.0f).withTrimmedTop (18.0f);
    const float radius = juce::jmin (area.getWidth() * 0.48f, area.getHeight() * 0.85f);
    const juce::Point<float> centre (area.getCentreX(), area.getY() + radius + 8.0f);

    constexpr float minDb = -40.0f, maxDb = 0.0f;
    constexpr float startAngle = -juce::MathConstants<float>::pi * 0.38f;
    constexpr float endAngle = juce::MathConstants<float>::pi * 0.38f;

    auto angleFor = [&] (float db)
    {
        const float norm = (juce::jlimit (minDb, maxDb, db) - minDb) / (maxDb - minDb);
        return startAngle + norm * (endAngle - startAngle);
    };

    auto pointAt = [&] (float angle, float rad)
    {
        return centre + juce::Point<float> (std::sin (angle) * rad, -std::cos (angle) * rad);
    };

    // Arc with a hot zone above -6 dBFS
    juce::Path arc;
    arc.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, startAngle, angleFor (-6.0f), true);
    g.setColour (Theme::grid);
    g.strokePath (arc, juce::PathStrokeType (3.0f));

    juce::Path hot;
    hot.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, angleFor (-6.0f), endAngle, true);
    g.setColour (Theme::over.withAlpha (0.7f));
    g.strokePath (hot, juce::PathStrokeType (3.0f));

    g.setFont (Theme::font (10.0f));
    for (float t : { -40.0f, -30.0f, -20.0f, -12.0f, -6.0f, -3.0f, 0.0f })
    {
        const float a = angleFor (t);
        g.setColour (Theme::textSecondary);
        g.drawLine ({ pointAt (a, radius - 8.0f), pointAt (a, radius) }, 1.0f);
        const auto p = pointAt (a, radius - 20.0f);
        g.drawText (juce::String (juce::roundToInt (t)), juce::Rectangle<float> (28.0f, 12.0f).withCentre (p),
                    juce::Justification::centred);
    }

    auto needle = [&] (float db, juce::Colour c, float thickness)
    {
        g.setColour (c);
        g.drawLine ({ centre, pointAt (angleFor (db), radius - 4.0f) }, thickness);
    };

    needle (average, Theme::textPrimary, 2.5f);
    needle (peak, Theme::secondary, 2.0f);

    g.setColour (Theme::panelEdge);
    g.fillEllipse (juce::Rectangle<float> (12.0f, 12.0f).withCentre (centre));

    auto legend = bounds.reduced (12.0f, 8.0f).removeFromBottom (14.0f);
    g.setFont (Theme::font (11.0f));
    g.setColour (Theme::secondary);
    g.drawText ("peak " + Theme::formatDb (peak), legend, juce::Justification::left);
    g.setColour (Theme::textPrimary);
    g.drawText ("avg " + Theme::formatDb (average), legend, juce::Justification::centred);
    g.setColour (Theme::textSecondary);
    const float crest = (peak > -149.0f && average > -149.0f) ? peak - average : 0.0f;
    g.drawText ("crest " + juce::String (crest, 1) + " dB", legend, juce::Justification::right);
}

//==============================================================================
void StereoPanel::setValues (float c, float b, float w)
{
    correlation = c;
    balance = b;
    width = w;
    repaint();
}

void StereoPanel::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    Theme::drawPanel (g, bounds, "Stereo");

    auto area = bounds.reduced (12.0f).withTrimmedTop (18.0f);
    auto barArea = area.removeFromTop (18.0f);

    g.setColour (Theme::background);
    g.fillRoundedRectangle (barArea, 3.0f);

    const float centreX = barArea.getCentreX();
    const float x = centreX + juce::jlimit (-1.0f, 1.0f, correlation) * barArea.getWidth() * 0.5f;
    g.setColour (correlation < 0.0f ? Theme::over : Theme::signal);
    g.fillRect (juce::Rectangle<float>::leftTopRightBottom (juce::jmin (centreX, x), barArea.getY() + 3.0f,
                                                            juce::jmax (centreX, x), barArea.getBottom() - 3.0f));
    g.setColour (Theme::textSecondary);
    g.drawVerticalLine (juce::roundToInt (centreX), barArea.getY(), barArea.getBottom());

    auto scaleRow = area.removeFromTop (14.0f);
    g.setFont (Theme::font (10.0f));
    g.drawText ("-1", scaleRow, juce::Justification::left);
    g.drawText ("0", scaleRow, juce::Justification::centred);
    g.drawText ("+1", scaleRow, juce::Justification::right);

    auto stats = area.removeFromTop (18.0f);
    g.setFont (Theme::font (12.0f));
    const auto third = stats.getWidth() / 3.0f;
    g.setColour (Theme::textPrimary);
    g.drawText ("corr " + juce::String (correlation, 2), stats.removeFromLeft (third), juce::Justification::left);
    g.drawText ("bal " + juce::String (balance, 1) + " dB", stats.removeFromLeft (third), juce::Justification::centred);
    g.drawText ("width " + juce::String (width, 2), stats, juce::Justification::right);
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
}

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
        }
    }
}

float SpectrumView::frequencyToX (float hz, float w) const noexcept
{
    constexpr float lo = 20.0f, hi = 20000.0f;
    return w * std::log (hz / lo) / std::log (hi / lo);
}

float SpectrumView::tiltAt (float hz) const noexcept
{
    // 4.5 dB/oct around 1 kHz, so a balanced mix reads roughly flat.
    return tilt ? 4.5f * std::log2 (hz / 1000.0f) : 0.0f;
}

void SpectrumView::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    Theme::drawPanel (g, bounds, tilt ? "Spectrum  (4.5 dB/oct tilt)" : "Spectrum");

    auto plot = bounds.reduced (12.0f).withTrimmedTop (18.0f).withTrimmedBottom (14.0f).withTrimmedLeft (26.0f);
    constexpr float minDb = -90.0f, maxDb = 6.0f;

    auto dbToY = [&] (float db)
    {
        const float norm = (juce::jlimit (minDb, maxDb, db) - minDb) / (maxDb - minDb);
        return plot.getBottom() - norm * plot.getHeight();
    };

    g.setFont (Theme::font (10.0f));
    for (float db = -84.0f; db <= 0.0f; db += 12.0f)
    {
        const float y = dbToY (db);
        g.setColour (Theme::grid);
        g.drawHorizontalLine (juce::roundToInt (y), plot.getX(), plot.getRight());
        g.setColour (Theme::textSecondary);
        g.drawText (juce::String (juce::roundToInt (db)), juce::Rectangle<float> (plot.getX() - 28.0f, y - 6.0f, 24.0f, 12.0f),
                    juce::Justification::centredRight);
    }

    for (float hz : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        const float x = plot.getX() + frequencyToX (hz, plot.getWidth());
        g.setColour (Theme::grid);
        g.drawVerticalLine (juce::roundToInt (x), plot.getY(), plot.getBottom());
        g.setColour (Theme::textSecondary);
        const auto label = hz >= 1000.0f ? juce::String (juce::roundToInt (hz / 1000.0f)) + "k" : juce::String (juce::roundToInt (hz));
        g.drawText (label, juce::Rectangle<float> (x - 20.0f, plot.getBottom() + 2.0f, 40.0f, 12.0f), juce::Justification::centred);
    }

    const auto& mags = analyzer.getMagnitudesDb();
    const auto& peaks = analyzer.getPeakHoldDb();
    const int numBins = analyzer.getNumBins();

    juce::Path line, fill, peakLine;
    bool started = false;

    // One point per horizontal pixel, taking the loudest bin under each pixel.
    const int width = juce::roundToInt (plot.getWidth());
    for (int px = 0; px <= width; px += 2)
    {
        const float hzLo = 20.0f * std::pow (1000.0f, static_cast<float> (px) / static_cast<float> (width));
        const float hzHi = 20.0f * std::pow (1000.0f, static_cast<float> (px + 2) / static_cast<float> (width));
        const int binLo = juce::jlimit (1, numBins - 1, static_cast<int> (hzLo * static_cast<float> (analyzer.getFftSize()) / static_cast<float> (sampleRate)));
        const int binHi = juce::jlimit (binLo, numBins - 1, static_cast<int> (hzHi * static_cast<float> (analyzer.getFftSize()) / static_cast<float> (sampleRate)));

        float level = -200.0f, peakLevel = -200.0f;
        for (int b = binLo; b <= binHi; ++b)
        {
            level = juce::jmax (level, mags[static_cast<size_t> (b)]);
            peakLevel = juce::jmax (peakLevel, peaks[static_cast<size_t> (b)]);
        }

        const float t = tiltAt (hzLo);
        const float x = plot.getX() + static_cast<float> (px);
        const float y = dbToY (level + t);
        const float py = dbToY (peakLevel + t);

        if (! started)
        {
            line.startNewSubPath (x, y);
            fill.startNewSubPath (x, plot.getBottom());
            fill.lineTo (x, y);
            peakLine.startNewSubPath (x, py);
            started = true;
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

    g.setColour (Theme::signalDim.withAlpha (0.25f));
    g.fillPath (fill);
    g.setColour (Theme::secondary.withAlpha (0.6f));
    g.strokePath (peakLine, juce::PathStrokeType (1.0f));
    g.setColour (Theme::signal);
    g.strokePath (line, juce::PathStrokeType (1.6f));
}
