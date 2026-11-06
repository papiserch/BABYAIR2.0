// BABY AIR - knob, trim knob and MIX slider drawing. Everything is drawn from the exact values JUCE passes in
// (sliderPos / proportion), so the visuals always represent the real parameter value.
#pragma once
#include "Palette.h"

namespace babyair::ui
{
inline float textWidth (const juce::Font& f, const juce::String& s)
{
    juce::GlyphArrangement ga;
    ga.addLineOfText (f, s, 0.0f, 0.0f);
    return ga.getBoundingBox (0, ga.getNumGlyphs(), true).getWidth();
}

inline float trackedWidth (const juce::Font& font, const juce::String& text, float tracking)
{
    float total = 0.0f;
    for (int i = 0; i < text.length(); ++i)
        total += textWidth (font, text.substring (i, i + 1)) + (text[i] == ' ' ? font.getHeight() * 0.25f : 0.0f)
                 + (i + 1 < text.length() ? tracking : 0.0f);
    return total;
}

// Letter-spaced text centred on (cx, cy)
inline void drawTracked (juce::Graphics& g, const juce::String& text, float cx, float cy, const juce::Font& font,
                         float tracking, juce::Colour colour)
{
    float total = 0.0f;
    juce::Array<float> w;
    for (int i = 0; i < text.length(); ++i)
    {
        w.add (textWidth (font, text.substring (i, i + 1)) + (text[i] == ' ' ? font.getHeight() * 0.25f : 0.0f));
        total += w.getLast() + (i + 1 < text.length() ? tracking : 0.0f);
    }
    float x = cx - total * 0.5f;
    g.setFont (font);
    g.setColour (colour);
    for (int i = 0; i < text.length(); ++i)
    {
        g.drawText (text.substring (i, i + 1), juce::Rectangle<float> (x - 2.0f, cy - font.getHeight(), w[i] + 4.0f, font.getHeight() * 2.0f),
                    juce::Justification::centred, false);
        x += w[i] + tracking;
    }
}

class BabyLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void setPalette (const Palette& p) { pal = p; }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                           float startAngle, float endAngle, juce::Slider& s) override
    {
        const bool small = (bool) s.getProperties().getWithDefault ("small", false);
        const auto b = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height);
        const float cx = b.getCentreX(), cy = b.getCentreY();
        const float angle = startAngle + pos * (endAngle - startAngle);

        if (small)   // TRIM knob (bottom-left): light knob with a thin indicator
        {
            const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f - 2.0f;
            juce::Path arc; arc.addCentredArc (cx, cy, r, r, 0.0f, startAngle, endAngle, true);
            g.setColour (juce::Colour (0xffd0d0d4)); g.strokePath (arc, juce::PathStrokeType (1.6f));
            juce::Path val; val.addCentredArc (cx, cy, r, r, 0.0f, startAngle, angle, true);
            g.setColour (pal.arcA); g.strokePath (val, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            juce::Path body; body.addEllipse (cx - r * 0.72f, cy - r * 0.72f, r * 1.44f, r * 1.44f);
            juce::DropShadow (juce::Colours::black.withAlpha (0.18f), 4, { 0, 2 }).drawForPath (g, body);
            g.setGradientFill (juce::ColourGradient (juce::Colours::white, cx - r * 0.3f, cy - r * 0.4f, juce::Colour (0xffe4e4e8), cx + r * 0.5f, cy + r * 0.6f, true));
            g.fillPath (body);
            g.setColour (juce::Colour (0xff2a2a2a));
            g.saveState(); g.addTransform (juce::AffineTransform::rotation (angle, cx, cy));
            g.fillRoundedRectangle (cx - 0.9f, cy - r * 0.68f, 1.8f, r * 0.42f, 0.9f);
            g.restoreState();
            return;
        }

        const float R = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f - 14.0f;    // arc radius
        const float rk = R * 0.80f;                                                   // knob body radius

        // track
        juce::Path track; track.addCentredArc (cx, cy, R, R, 0.0f, startAngle, endAngle, true);
        g.setColour (juce::Colour (0xffd9d8dc));
        g.strokePath (track, juce::PathStrokeType (5.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // value arc: gradient arcA -> arcB along the full travel (so colour == position), with soft glow
        if (pos > 0.002f)
        {
            const int steps = 64;
            for (int pass = 0; pass < 2; ++pass)
                for (int i = 0; i < steps; ++i)
                {
                    const float t0 = (float) i / steps, t1 = (float) (i + 1) / steps;
                    const float a0 = startAngle + t0 * (endAngle - startAngle);
                    if (a0 >= angle) break;
                    const float a1 = juce::jmin (angle, startAngle + t1 * (endAngle - startAngle) + 0.01f);
                    juce::Path seg; seg.addCentredArc (cx, cy, R, R, 0.0f, a0, a1, true);
                    const auto c = pal.arcA.interpolatedWith (pal.arcB, (t0 + t1) * 0.5f);
                    if (pass == 0) { g.setColour (c.withAlpha (0.20f)); g.strokePath (seg, juce::PathStrokeType (13.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)); }
                    else           { g.setColour (c);                   g.strokePath (seg, juce::PathStrokeType (5.5f,  juce::PathStrokeType::curved, juce::PathStrokeType::rounded)); }
                }
        }

        // knob body
        juce::Path body; body.addEllipse (cx - rk, cy - rk, rk * 2.0f, rk * 2.0f);
        juce::DropShadow (juce::Colours::black.withAlpha (0.38f), 18, { 0, 9 }).drawForPath (g, body);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4b4b4d), cx - rk * 0.45f, cy - rk * 0.6f,
                                                 juce::Colour (0xff1b1b1c), cx + rk * 0.6f, cy + rk * 0.8f, true));
        g.fillPath (body);
        g.setColour (juce::Colours::white.withAlpha (0.10f));
        g.drawEllipse (cx - rk + 0.5f, cy - rk + 0.5f, rk * 2.0f - 1.0f, rk * 2.0f - 1.0f, 1.0f);

        // indicator
        g.setColour (juce::Colours::white);
        g.saveState(); g.addTransform (juce::AffineTransform::rotation (angle, cx, cy));
        g.fillRoundedRectangle (cx - 2.2f, cy - rk * 0.94f, 4.4f, rk * 0.38f, 2.2f);
        g.restoreState();
    }

    int getSliderThumbRadius (juce::Slider&) override { return 12; }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos, float, float,
                           juce::Slider::SliderStyle, juce::Slider&) override
    {
        const float cy = (float) y + (float) height * 0.5f, th = 8.0f;
        const juce::Rectangle<float> track ((float) x, cy - th * 0.5f, (float) width, th);
        g.setColour (juce::Colour (0xffe0dfe3));
        g.fillRoundedRectangle (track, th * 0.5f);
        const auto filled = track.withRight (juce::jmax (track.getX() + th, sliderPos));
        g.setGradientFill (juce::ColourGradient (pal.arcA, track.getX(), 0.0f, pal.arcB, track.getRight(), 0.0f, false));
        g.fillRoundedRectangle (filled, th * 0.5f);
        juce::Path thumb; thumb.addEllipse (sliderPos - 12.0f, cy - 12.0f, 24.0f, 24.0f);
        juce::DropShadow (juce::Colours::black.withAlpha (0.28f), 8, { 0, 3 }).drawForPath (g, thumb);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white, sliderPos - 4.0f, cy - 8.0f, juce::Colour (0xffe6e6ea), sliderPos + 6.0f, cy + 12.0f, true));
        g.fillPath (thumb);
    }

private:
    Palette pal = paletteFor (0);
};
} // namespace babyair::ui
