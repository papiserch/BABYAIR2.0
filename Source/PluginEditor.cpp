#include "PluginEditor.h"

using namespace babyair;
using namespace babyair::ui;

namespace
{
juce::RangedAudioParameter& param (BabyAirProcessor& p, const char* id) { return *p.apvts.getParameter (id); }
const juce::Colour kInk (0xff222226), kGrey (0xff77777d);
} // namespace

BabyAirPanel::BabyAirPanel (BabyAirProcessor& p)
    : proc (p),
      midAtt  (p.apvts, ids::midHigh, midKnob),
      highAtt (p.apvts, ids::high, highKnob),
      trimAtt (p.apvts, ids::trim, trimKnob),
      mixAtt  (p.apvts, ids::mix, mixSlider),
      modeAtt  (param (p, ids::airMode), [this] (float v) { onModeChanged ((int) std::lround (v), false); }),
      powerAtt (param (p, ids::power),   [this] (float v) { onChip.setState (v > 0.5f, v > 0.5f ? "on" : "off"); }),
      linkAtt  (param (p, ids::link),    [this] (float v) { linkActive = v > 0.5f; linkChip.setState (linkActive, "LINK"); }),
      qualityAtt (param (p, ids::quality), [this] (float v) { quality = (int) std::lround (v); qualityChip.setState (false, "OS " + juce::String (2 << quality) + "x"); })
{
    pLink = &param (p, ids::link); pMid = &param (p, ids::midHigh); pHigh = &param (p, ids::high);
    setOpaque (false);
    setSize (kDesignW, kDesignH);

    for (auto* s : { &midKnob, &highKnob, &trimKnob })
    {
        s->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s->setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        s->setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        s->setMouseDragSensitivity (240);
        s->setVelocityBasedMode (false);
        s->setLookAndFeel (&laf);
        addAndMakeVisible (*s);
    }
    trimKnob.getProperties().set ("small", true);
    midKnob.setDoubleClickReturnValue (true, 50.0);
    highKnob.setDoubleClickReturnValue (true, 50.0);
    trimKnob.setDoubleClickReturnValue (true, 0.0);
    trimKnob.setMouseDragSensitivity (160);

    mixSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    mixSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    mixSlider.setDoubleClickReturnValue (true, 100.0);
    mixSlider.setLookAndFeel (&laf);
    addAndMakeVisible (mixSlider);

    // LINK: keeps the difference between MID HIGH and HIGH while the user drags either knob.
    midKnob.onDragStart  = [this] { beginLinkDrag (true); };
    highKnob.onDragStart = [this] { beginLinkDrag (false); };
    midKnob.onDragEnd = highKnob.onDragEnd = [this] { endLinkDrag(); };
    midKnob.onValueChange  = [this] { linkMoved (true);  valueChanged(); };
    highKnob.onValueChange = [this] { linkMoved (false); valueChanged(); };
    trimKnob.onValueChange = mixSlider.onValueChange = [this] { repaint(); };

    addAndMakeVisible (modeSel);
    modeSel.onSelect = [this] (int m) { modeAtt.setValueAsCompleteGesture ((float) m); };

    onChip.onClick = [this] { powerAtt.setValueAsCompleteGesture (onChip.on ? 0.0f : 1.0f); };
    linkChip.onClick = [this] { linkAtt.setValueAsCompleteGesture (linkActive ? 0.0f : 1.0f); };
    qualityChip.showLed = false;
    qualityChip.onClick = [this] { qualityAtt.setValueAsCompleteGesture ((float) ((quality + 1) % 4)); };
    for (auto* c : { &onChip, &linkChip, &qualityChip }) addAndMakeVisible (*c);

    for (auto* c : { (juce::Component*) &inMeter, (juce::Component*) &outMeter, (juce::Component*) &art, (juce::Component*) &logo })
        addAndMakeVisible (*c);

    modeAtt.sendInitialUpdate(); powerAtt.sendInitialUpdate(); linkAtt.sendInitialUpdate(); qualityAtt.sendInitialUpdate();
    onModeChanged (mode, true);
    valueChanged();
}

BabyAirPanel::~BabyAirPanel()
{
    for (auto* s : { &midKnob, &highKnob, &trimKnob, &mixSlider }) s->setLookAndFeel (nullptr);
}

void BabyAirPanel::onModeChanged (int m, bool immediate)
{
    mode = juce::jlimit (0, 3, m);
    const bool neverShown = (shown.arcA == juce::Colour());     // first call: no previous palette to fade from
    fromPal = shown;
    toPal = paletteFor (mode);
    palProgress = (immediate || neverShown) ? 1.0f : 0.0f;
    if (palProgress >= 1.0f) shown = toPal;
    applyPalette();
}

void BabyAirPanel::applyPalette()
{
    laf.setPalette (shown);
    inMeter.setPalette (shown); outMeter.setPalette (shown);
    art.setPalette (shown);
    modeSel.setSelected (mode, shown.arcA);
    for (auto* c : { (juce::Component*) &midKnob, (juce::Component*) &highKnob, (juce::Component*) &trimKnob, (juce::Component*) &mixSlider })
        c->repaint();
}

void BabyAirPanel::tick (float dt)
{
    if (palProgress < 1.0f)
    {
        palProgress = juce::jmin (1.0f, palProgress + dt / 0.35f);
        const float t = palProgress * palProgress * (3.0f - 2.0f * palProgress);
        shown = lerp (fromPal, toPal, t);
        applyPalette();
    }
    inMeter.update (proc.inputMeter.read(), dt);
    outMeter.update (proc.outputMeter.read(), dt);
}

void BabyAirPanel::valueChanged()
{
    art.setKnobs ((float) midKnob.getValue() * 0.01f, (float) highKnob.getValue() * 0.01f);
    repaint (200, 270, 480, 40);
}

// ------------------------------------------------------------------------------------------------ LINK
void BabyAirPanel::beginLinkDrag (bool fromMid)
{
    linkActive = pLink->getValue() > 0.5f;
    linkDragging = true;
    linkOffset = (float) (highKnob.getValue() - midKnob.getValue());   // difference preserved while dragging
    linkOther = nullptr;
    if (linkActive) { linkOther = fromMid ? pHigh : pMid; linkOther->beginChangeGesture(); }
}

void BabyAirPanel::linkMoved (bool fromMid)
{
    if (! linkActive || ! linkDragging || inLink) return;
    const juce::ScopedValueSetter<bool> guard (inLink, true);
    auto& src = fromMid ? midKnob : highKnob;
    auto* other = fromMid ? pHigh : pMid;
    const float d = fromMid ? linkOffset : -linkOffset;                // other = src + d
    const float lo = juce::jmax (0.0f, -d), hi = juce::jmin (100.0f, 100.0f - d);
    const float v = juce::jlimit (lo, hi, (float) src.getValue());     // clamp the dragged knob so the gap never changes
    if (std::abs (v - (float) src.getValue()) > 1.0e-4f) src.setValue (v, juce::sendNotificationSync);
    other->setValueNotifyingHost (other->convertTo0to1 (v + d));
}

void BabyAirPanel::endLinkDrag()
{
    if (linkOther != nullptr) linkOther->endChangeGesture();   // balances beginLinkDrag; the dragged knob's own gesture is handled by its attachment
    linkOther = nullptr;
    linkDragging = false;
}

// ------------------------------------------------------------------------------------------------ layout / paint
void BabyAirPanel::resized()
{
    logo.setBounds (20, 14, 58, 42);
    qualityChip.setBounds (716, 17, 70, 24);
    onChip.setBounds (800, 17, 56, 24);
    modeSel.setBounds (20, 85, 152, 235);
    inMeter.setBounds (216, 125, 10, 145);
    outMeter.setBounds (654, 125, 10, 145);
    midKnob.setBounds (232, 107, 188, 188);
    highKnob.setBounds (457, 107, 188, 188);
    linkChip.setBounds (414, 74, 62, 23);
    art.setBounds (689, 50, 273, 277);
    trimKnob.setBounds (34, 362, 32, 32);
    mixSlider.setBounds (685, 366, 208, 24);
}

void BabyAirPanel::paint (juce::Graphics& g)
{
    const auto area = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffaf8f5), 0.0f, 0.0f, juce::Colour (0xffefece8), 0.0f, area.getHeight(), false));
    g.fillRoundedRectangle (area, 26.0f);
    g.setColour (juce::Colours::white.withAlpha (0.8f));
    g.drawRoundedRectangle (area.reduced (0.5f), 26.0f, 1.0f);

    const juce::Font small (juce::FontOptions (11.0f)), label (juce::FontOptions (13.5f)), value (juce::FontOptions (19.0f));

    // title block
    const juce::Font title (juce::FontOptions (22.0f));
    const float tw = trackedWidth (title, "BABY AIR", 4.5f);
    drawTracked (g, "BABY AIR", 93.0f + tw * 0.5f, 31.0f, title, 4.5f, kInk);
    g.setColour (kInk); g.setFont (juce::Font (juce::FontOptions (9.0f)));
    g.drawText ("TM", juce::Rectangle<float> (93.0f + tw + 2.0f, 15.0f, 20.0f, 12.0f), juce::Justification::centredLeft, false);
    const juce::Font sub (juce::FontOptions (13.0f));
    drawTracked (g, "harmonic exciter", 93.0f + trackedWidth (sub, "harmonic exciter", 2.6f) * 0.5f, 51.0f, sub, 2.6f, kInk);

    // header right
    g.setColour (juce::Colour (0xffd8d7dc)); g.drawLine (876.0f, 20.0f, 876.0f, 40.0f, 1.0f);
    g.setColour (kGrey); g.setFont (juce::Font (juce::FontOptions (13.0f)));
    g.drawText ("v1.0.0", juce::Rectangle<float> (886.0f, 17.0f, 70.0f, 24.0f), juce::Justification::centred, false);

    // dividers
    g.setColour (juce::Colour (0xffdddce0));
    g.drawLine (184.0f, 103.0f, 184.0f, 311.0f, 1.0f);
    g.drawLine (26.0f, 342.0f, 951.0f, 342.0f, 1.0f);
    g.setColour (juce::Colour (0xffe2e1e5));
    g.drawLine (150.0f, 378.0f, 660.0f, 378.0f, 1.0f);

    // knob labels / values
    drawTracked (g, "MID HIGH", 326.0f, 101.0f, label, 3.0f, kInk);
    drawTracked (g, "HIGH", 551.0f, 101.0f, label, 3.0f, kInk);
    g.setFont (value); g.setColour (kInk);
    g.drawText (juce::String (midKnob.getValue(), 1) + "%",  juce::Rectangle<float> (276.0f, 276.0f, 100.0f, 30.0f), juce::Justification::centred, false);
    g.drawText (juce::String (highKnob.getValue(), 1) + "%", juce::Rectangle<float> (501.0f, 276.0f, 100.0f, 30.0f), juce::Justification::centred, false);
    g.setFont (juce::Font (juce::FontOptions (15.0f))); g.setColour (kGrey);
    g.drawText ("-", juce::Rectangle<float> (262.0f, 276.0f, 14.0f, 30.0f), juce::Justification::centred, false);
    g.drawText ("+", juce::Rectangle<float> (376.0f, 276.0f, 14.0f, 30.0f), juce::Justification::centred, false);
    g.drawText ("-", juce::Rectangle<float> (487.0f, 276.0f, 14.0f, 30.0f), juce::Justification::centred, false);
    g.drawText ("+", juce::Rectangle<float> (601.0f, 276.0f, 14.0f, 30.0f), juce::Justification::centred, false);

    drawTracked (g, "IN",  221.0f, 283.0f, small, 1.5f, kGrey);
    drawTracked (g, "OUT", 659.0f, 283.0f, small, 1.5f, kGrey);

    // bottom row
    drawTracked (g, "TRIM", 37.0f + trackedWidth (small, "TRIM", 2.4f) * 0.5f, 353.0f, small, 2.4f, kInk);
    g.setFont (juce::Font (juce::FontOptions (12.0f))); g.setColour (kGrey);
    g.drawText (juce::String (trimKnob.getValue(), 1) + " dB", juce::Rectangle<float> (72.0f, 366.0f, 70.0f, 24.0f), juce::Justification::centredLeft, false);
    drawTracked (g, "MIX", 685.0f + trackedWidth (label, "MIX", 3.0f) * 0.5f, 361.0f, label, 3.0f, kInk);
    g.setFont (juce::Font (juce::FontOptions (15.0f))); g.setColour (kInk);
    g.drawText (juce::String (juce::roundToInt (mixSlider.getValue())) + "%", juce::Rectangle<float> (900.0f, 366.0f, 60.0f, 24.0f), juce::Justification::centred, false);
}

// ------------------------------------------------------------------------------------------------ editor
BabyAirEditor::BabyAirEditor (BabyAirProcessor& p) : AudioProcessorEditor (&p), panel (p)
{
    addAndMakeVisible (panel);
    setResizable (true, true);
    setResizeLimits (BabyAirPanel::kDesignW / 2, BabyAirPanel::kDesignH / 2, BabyAirPanel::kDesignW * 2, BabyAirPanel::kDesignH * 2);
    getConstrainer()->setFixedAspectRatio ((double) BabyAirPanel::kDesignW / (double) BabyAirPanel::kDesignH);
    setSize (BabyAirPanel::kDesignW, BabyAirPanel::kDesignH);
    startTimerHz (30);
}

void BabyAirEditor::resized()
{
    panel.setBounds (0, 0, BabyAirPanel::kDesignW, BabyAirPanel::kDesignH);
    panel.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) BabyAirPanel::kDesignW));
}
