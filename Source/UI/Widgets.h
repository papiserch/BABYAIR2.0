// BABY AIR - custom widgets: meter, pill toggle, mode selector, logo, gradient art
#pragma once
#include "BabyLookAndFeel.h"
#include "../DSP/MeteringEngine.h"
#include "../DSP/DspCommon.h"

namespace babyair::ui
{
// ------------------------------------------------------------------------------------------------ meter
// Segmented level meter (RMS bar + peak marker + peak hold + clip LED). Fed by MeteringEngine::Reading.
class MeterBar : public juce::Component
{
public:
    void setPalette (const Palette& p) { pal = p; repaint(); }

    void update (const babyair::MeteringEngine::Reading& r, float dt)
    {
        const float rmsDb = babyair::linToDb (r.rms), pkDb = babyair::linToDb (r.peak);
        level = rmsDb > level ? level + (rmsDb - level) * 0.6f : juce::jmax (rmsDb, level - 18.0f * dt);
        peak  = pkDb  > peak  ? pkDb : juce::jmax (pkDb, peak - 30.0f * dt);
        if (pkDb >= hold) { hold = pkDb; holdTimer = 1.5f; }
        else if (holdTimer > 0.0f) holdTimer -= dt;
        else hold = juce::jmax (-90.0f, hold - 20.0f * dt);
        if (r.clipped) clipTimer = 2.0f; else clipTimer = juce::jmax (0.0f, clipTimer - dt);
        repaint();
    }

    void mouseDown (const juce::MouseEvent&) override { clipTimer = 0.0f; hold = -90.0f; repaint(); }

    void paint (juce::Graphics& g) override
    {
        const int n = 22;
        const auto b = getLocalBounds().toFloat();
        const float segH = (b.getHeight() - 10.0f) / n, w = b.getWidth();
        auto segOf = [n] (float db) { return juce::jlimit (-1, n - 1, (int) std::floor ((db + 60.0f) / 60.0f * n)); };
        const int lit = segOf (level), pk = segOf (peak), hd = segOf (hold);
        for (int i = 0; i < n; ++i)
        {
            const float y = b.getBottom() - (float) (i + 1) * segH;
            const juce::Rectangle<float> r (0.0f, y + 0.8f, w, segH - 1.6f);
            const auto col = pal.arcA.interpolatedWith (pal.arcB, (float) i / (n - 1));
            if (i <= lit)        g.setColour (col);
            else if (i == pk || i == hd) g.setColour (col.withAlpha (0.75f));
            else                 g.setColour (juce::Colour (0xffdcdbe0));
            g.fillRoundedRectangle (r, 1.2f);
        }
        g.setColour (clipTimer > 0.0f ? juce::Colour (0xffff3b3b) : juce::Colour (0xffd0cfd4));
        g.fillEllipse (w * 0.5f - 3.0f, 0.0f, 6.0f, 6.0f);
    }

private:
    Palette pal = paletteFor (0);
    float level = -90.0f, peak = -90.0f, hold = -90.0f, holdTimer = 0.0f, clipTimer = 0.0f;
};

// ------------------------------------------------------------------------------------------------ pill toggle
class PillToggle : public juce::Component
{
public:
    std::function<void()> onClick;
    juce::String text;
    bool showLed = true, on = false;

    void setState (bool s, const juce::String& t) { on = s; text = t; repaint(); }
    void mouseUp (const juce::MouseEvent& e) override { if (contains (e.getPosition()) && onClick) onClick(); }
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit  (const juce::MouseEvent&) override { hover = false; repaint(); }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (juce::Colours::white.withAlpha (hover ? 0.95f : 0.7f));
        g.fillRoundedRectangle (b, b.getHeight() * 0.5f);
        g.setColour (juce::Colour (0xffd8d7dc));
        g.drawRoundedRectangle (b, b.getHeight() * 0.5f, 1.0f);
        float tx = b.getX();
        if (showLed)
        {
            const float d = 11.0f, lx = b.getX() + 10.0f, ly = b.getCentreY() - d * 0.5f;
            if (on) { g.setColour (juce::Colour (0xff6ee01a).withAlpha (0.30f)); g.fillEllipse (lx - 3, ly - 3, d + 6, d + 6); }
            g.setColour (on ? juce::Colour (0xff6ee01a) : juce::Colour (0xffbdbcc2));
            g.fillEllipse (lx, ly, d, d);
            tx = lx + d;
        }
        g.setColour (juce::Colour (0xff2a2a2d));
        g.setFont (juce::Font (juce::FontOptions (12.0f)));
        g.drawText (text, juce::Rectangle<float> (tx, b.getY(), b.getRight() - tx - 4.0f, b.getHeight()), juce::Justification::centred, false);
    }
private:
    bool hover = false;
};

// ------------------------------------------------------------------------------------------------ mode selector
class ModeSelector : public juce::Component
{
public:
    std::function<void (int)> onSelect;
    void setSelected (int m, juce::Colour accent) { sel = m; accentCol = accent; repaint(); }

    static constexpr float kDotY[4] = { 14.0f, 34.0f, 55.0f, 75.0f };
    static constexpr float kRowY0 = 113.0f, kRowH = 28.0f;

    void mouseUp (const juce::MouseEvent& e) override
    {
        const auto p = e.position;
        if (p.y >= kRowY0 - 4.0f && p.y < kRowY0 - 4.0f + 4.0f * kRowH && onSelect)
            onSelect (juce::jlimit (0, 3, (int) ((p.y - (kRowY0 - 4.0f)) / kRowH)));
        else if (p.y < 90.0f && onSelect)
            for (int i = 0; i < 4; ++i) if (std::abs (p.y - kDotY[i]) < 10.0f) onSelect (i);
    }

    void paint (juce::Graphics& g) override
    {
        static const char* names[4] = { "CLEAN", "BRIGHT", "VINTAGE", "BABY BOOST" };
        for (int i = 0; i < 4; ++i)
        {
            const float cx = 16.0f;
            if (i == sel) { g.setColour (accentCol.withAlpha (0.30f)); g.fillEllipse (cx - 8, kDotY[i] - 8, 16, 16); }
            g.setColour (i == sel ? accentCol : juce::Colour (0xffbdbcc2));
            g.fillEllipse (cx - 4.5f, kDotY[i] - 4.5f, 9.0f, 9.0f);
        }
        drawTracked (g, "AIR MODE", 76.0f, 91.0f, juce::Font (juce::FontOptions (11.5f)), 2.4f, juce::Colour (0xff55555a));
        for (int i = 0; i < 4; ++i)
        {
            const float cy = kRowY0 + (float) i * kRowH + kRowH * 0.5f - 4.0f;
            if (i == sel)
            {
                g.setColour (juce::Colour (0xff1d1d20));
                g.fillRoundedRectangle (juce::Rectangle<float> (26.0f, cy - 13.0f, 100.0f, 26.0f), 13.0f);
            }
            drawTracked (g, names[i], 76.0f, cy, juce::Font (juce::FontOptions (12.5f)), 2.6f,
                         i == sel ? juce::Colours::white : juce::Colour (0xff3a3a3e));
        }
    }
private:
    int sel = 0;
    juce::Colour accentCol = juce::Colour (0xff3ee8d0);
};

// ------------------------------------------------------------------------------------------------ logo
// Vector "Baby Demon" mascot (drawn with Paths, scales without blur).
class LogoComponent : public juce::Component
{
public:
    void paint (juce::Graphics& g) override
    {
        const float s = juce::jmin (getWidth() / 100.0f, getHeight() / 72.0f);
        g.addTransform (juce::AffineTransform::scale (s));
        const auto ink = juce::Colour (0xff2a2a2d);
        auto wing = [&] (bool right)
        {
            juce::Path w;
            w.startNewSubPath (29, 40); w.quadraticTo (10, 24, 1, 36); w.quadraticTo (8, 38, 6, 45);
            w.quadraticTo (14, 44, 14, 53); w.quadraticTo (23, 50, 31, 51); w.closeSubPath();
            if (right) w.applyTransform (juce::AffineTransform::scale (-1.0f, 1.0f).translated (100.0f, 0.0f));
            g.setColour (ink); g.fillPath (w);
        };
        wing (false); wing (true);
        for (int side = 0; side < 2; ++side)
        {
            juce::Path h; h.startNewSubPath (34, 25); h.lineTo (29, 5); h.lineTo (43, 19); h.closeSubPath();
            if (side) h.applyTransform (juce::AffineTransform::scale (-1.0f, 1.0f).translated (100.0f, 0.0f));
            g.setColour (ink); g.fillPath (h);
        }
        g.setColour (juce::Colours::white); g.fillEllipse (28, 20, 44, 40);
        g.setColour (ink); g.drawEllipse (28, 20, 44, 40, 2.4f);
        for (float ex : { 40.0f, 60.0f })
        {
            g.setColour (juce::Colours::white); g.fillEllipse (ex - 6, 36, 12, 11);
            g.setColour (ink); g.drawEllipse (ex - 6, 36, 12, 11, 1.6f);
            g.fillEllipse (ex - 2, 41, 4.5f, 4.5f);
            g.fillRect (ex - 7.0f, 38.5f, 14.0f, 2.6f);    // heavy half-lid
        }
        g.drawLine (46, 52, 54, 52, 1.8f);
    }
};

// ------------------------------------------------------------------------------------------------ gradient art
// Smooth "mesh" gradient: normalised Gaussian blobs rendered into a small image and up-scaled with high
// quality interpolation (gives the soft blur). The dark core drifts subtly with the MID HIGH / HIGH knobs.
class GradientArt : public juce::Component
{
public:
    void setPalette (const Palette& p) { pal = p; rebuild(); }
    void setKnobs (float mid01, float high01) { mid = mid01; high = high01; rebuild(); }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        juce::Path clip; clip.addRoundedRectangle (b, 22.0f);
        juce::DropShadow (juce::Colours::black.withAlpha (0.16f), 14, { 0, 6 }).drawForPath (g, clip);
        {
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (clip);
            g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
            g.drawImage (img, b);
        }
        // sparkle
        const float sx = b.getRight() - 34.0f, sy = b.getY() + 34.0f, r = 11.0f;
        juce::Path star;
        star.startNewSubPath (sx, sy - r); star.quadraticTo (sx, sy, sx + r * 0.55f, sy);
        star.quadraticTo (sx, sy, sx, sy + r); star.quadraticTo (sx, sy, sx - r * 0.55f, sy);
        star.quadraticTo (sx, sy, sx, sy - r); star.closeSubPath();
        g.setColour (juce::Colours::white.withAlpha (0.92f)); g.fillPath (star);
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.drawRoundedRectangle (b.reduced (0.5f), 22.0f, 1.0f);
    }
    void resized() override { rebuild(); }

private:
    void rebuild()
    {
        constexpr int S = 72;
        if (! img.isValid()) img = juce::Image (juce::Image::ARGB, S, S, false);
        struct Blob { float u, v, sigma, weight; };
        const Blob blobs[5] = { { 0.02f, 0.05f, 0.38f, 1.0f },
                                { 0.60f, 0.30f, 0.30f, 1.0f },
                                { 0.72f + 0.12f * (high - 0.5f), 0.60f + 0.12f * (mid - 0.5f), 0.22f, 1.7f },
                                { 0.95f, 0.98f, 0.30f, 1.0f },
                                { 0.05f, 0.95f, 0.30f, 1.0f } };
        juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
        for (int y = 0; y < S; ++y)
            for (int x = 0; x < S; ++x)
            {
                const float u = (float) x / (S - 1), v = (float) y / (S - 1);
                float wsum = 0.0f, r = 0.0f, gg = 0.0f, b = 0.0f;
                for (int i = 0; i < 5; ++i)
                {
                    const float dx = u - blobs[i].u, dy = v - blobs[i].v;
                    const float w = blobs[i].weight * std::exp (-(dx * dx + dy * dy) / (2.0f * blobs[i].sigma * blobs[i].sigma)) + 1.0e-4f;
                    wsum += w; r += w * pal.mesh[i].getFloatRed(); gg += w * pal.mesh[i].getFloatGreen(); b += w * pal.mesh[i].getFloatBlue();
                }
                const unsigned h = (unsigned) (x * 73856093) ^ (unsigned) (y * 19349663);
                const float grain = ((float) (h % 1000) / 1000.0f - 0.5f) * 0.03f;   // fixed film grain, not animated
                bd.setPixelColour (x, y, juce::Colour::fromFloatRGBA (juce::jlimit (0.0f, 1.0f, r / wsum + grain),
                                                                      juce::jlimit (0.0f, 1.0f, gg / wsum + grain),
                                                                      juce::jlimit (0.0f, 1.0f, b / wsum + grain), 1.0f));
            }
        repaint();
    }

    Palette pal = paletteFor (0);
    float mid = 0.5f, high = 0.5f;
    juce::Image img;
};
} // namespace babyair::ui
