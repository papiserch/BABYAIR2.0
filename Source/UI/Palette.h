// BABY AIR - per-mode colour palettes
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace babyair::ui
{
struct Palette
{
    juce::Colour arcA, arcB;     // knob arc / meter / mix gradient (start -> end)
    juce::Colour mesh[5];        // gradient art: base, hot, dark core, deep, soft
};

inline Palette lerp (const Palette& a, const Palette& b, float t)
{
    Palette r;
    r.arcA = a.arcA.interpolatedWith (b.arcA, t);
    r.arcB = a.arcB.interpolatedWith (b.arcB, t);
    for (int i = 0; i < 5; ++i) r.mesh[i] = a.mesh[i].interpolatedWith (b.mesh[i], t);
    return r;
}

inline const Palette& paletteFor (int mode)
{
    static const Palette kPalettes[4] =
    {
        // CLEAN: turquoise / cyan / light blue / soft violet / pink
        { juce::Colour (0xff3ee8d0), juce::Colour (0xffff6ad5),
          { juce::Colour (0xfff4eae6), juce::Colour (0xffff4f93), juce::Colour (0xff120f1d), juce::Colour (0xff4a4fd0), juce::Colour (0xfff7a3c4) } },
        // BRIGHT: electric blue / royal blue / indigo / violet
        { juce::Colour (0xff2f6bff), juce::Colour (0xff9b5cff),
          { juce::Colour (0xffeaf0ff), juce::Colour (0xff2d62ff), juce::Colour (0xff0d0a38), juce::Colour (0xff8a3ffc), juce::Colour (0xffbac8ff) } },
        // VINTAGE: pale yellow / gold / amber / orange
        { juce::Colour (0xffffd45e), juce::Colour (0xffff8a2b),
          { juce::Colour (0xfffff3d4), juce::Colour (0xffffb52e), juce::Colour (0xff2a1606), juce::Colour (0xffff7a1a), juce::Colour (0xffffd9a6) } },
        // BABY BOOST: fluorescent green / purple / electric blue (infrared-neon)
        { juce::Colour (0xff7dff3a), juce::Colour (0xffb24bff),
          { juce::Colour (0xffe6ffe0), juce::Colour (0xff39ff14), juce::Colour (0xff0b0420), juce::Colour (0xff8f2cff), juce::Colour (0xff2d6bff) } },
    };
    return kPalettes[juce::jlimit (0, 3, mode)];
}
} // namespace babyair::ui
