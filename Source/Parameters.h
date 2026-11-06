// BABY AIR - parameter IDs and layout
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace babyair::ids
{
inline constexpr const char* power   = "power";
inline constexpr const char* airMode = "airMode";
inline constexpr const char* midHigh = "midHigh";
inline constexpr const char* high    = "high";
inline constexpr const char* link    = "link";
inline constexpr const char* trim    = "trim";
inline constexpr const char* mix     = "mix";
inline constexpr const char* quality = "quality";
} // namespace babyair::ids

namespace babyair
{
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
}
