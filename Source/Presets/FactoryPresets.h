// BABY AIR - factory presets (exposed to the host as programs)
#pragma once

namespace babyair
{
struct FactoryPreset
{
    const char* name;
    int mode;           // 0 Clean, 1 Bright, 2 Vintage, 3 Baby Boost
    float midHigh, high; // %
    bool link;
    float trimDb, mix;  // dB, %
};

inline constexpr FactoryPreset kFactoryPresets[] =
{
    { "Clean Vocal",          0, 40.0f, 50.0f, false,  0.0f, 100.0f },
    { "Bright Vocal",         1, 45.0f, 40.0f, false,  0.0f, 100.0f },
    { "Trap Vocal",           3, 55.0f, 60.0f, false, -1.0f, 100.0f },
    { "Reggaeton Vocal",      3, 60.0f, 45.0f, false, -1.0f,  90.0f },
    { "Airy Adlibs",          3, 30.0f, 78.0f, false, -1.0f, 100.0f },
    { "Modern Hi-Hats",       1, 20.0f, 70.0f, false,  0.0f,  85.0f },
    { "Vintage Air",          2, 50.0f, 50.0f, false,  0.0f, 100.0f },
    { "Baby Boost Signature", 3, 52.3f, 78.7f, false, -1.5f, 100.0f },
    { "Urban Master Bus",     3, 22.0f, 30.0f, true,   0.0f,  70.0f },
};
inline constexpr int kNumFactoryPresets = (int) (sizeof (kFactoryPresets) / sizeof (kFactoryPresets[0]));
} // namespace babyair
