#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "UI/Widgets.h"

// The whole interface lives in a fixed 980x410 design space (BabyAirPanel). BabyAirEditor scales it to any
// window size (fixed aspect ratio), so layout and typography stay crisp at every resolution.
class BabyAirPanel : public juce::Component
{
public:
    static constexpr int kDesignW = 980, kDesignH = 410;

    explicit BabyAirPanel (BabyAirProcessor&);
    ~BabyAirPanel() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void tick (float dtSeconds);          // 30 Hz: meters, palette crossfade

private:
    void onModeChanged (int mode, bool immediate);
    void applyPalette();
    void beginLinkDrag (bool fromMid);
    void linkMoved (bool fromMid);
    void endLinkDrag();
    void valueChanged();

    BabyAirProcessor& proc;
    babyair::ui::BabyLookAndFeel laf;
    juce::Slider midKnob, highKnob, trimKnob, mixSlider;
    babyair::ui::ModeSelector modeSel;
    babyair::ui::PillToggle onChip, linkChip, qualityChip;
    babyair::ui::MeterBar inMeter, outMeter;
    babyair::ui::GradientArt art;
    babyair::ui::LogoComponent logo;

    babyair::ui::Palette fromPal, toPal, shown;
    float palProgress = 1.0f;
    int mode = 0, quality = 2;

    juce::RangedAudioParameter *pLink = nullptr, *pMid = nullptr, *pHigh = nullptr, *linkOther = nullptr;
    bool linkActive = false, linkDragging = false, inLink = false;
    float linkOffset = 0.0f;

    juce::AudioProcessorValueTreeState::SliderAttachment midAtt, highAtt, trimAtt, mixAtt;
    juce::ParameterAttachment modeAtt, powerAtt, linkAtt, qualityAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BabyAirPanel)
};

class BabyAirEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit BabyAirEditor (BabyAirProcessor&);
    ~BabyAirEditor() override { stopTimer(); }
    void resized() override;

private:
    void timerCallback() override { panel.tick (1.0f / 30.0f); }
    BabyAirPanel panel;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BabyAirEditor)
};
