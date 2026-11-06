#include "Parameters.h"

namespace babyair
{
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using juce::ParameterID;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto percentText = [] (float v, int) { return juce::String (v, 1) + "%"; };
    auto percentParse = [] (const juce::String& t) { return t.retainCharacters ("0123456789.-").getFloatValue(); };

    layout.add (std::make_unique<juce::AudioParameterBool> (ParameterID { ids::power, 1 }, "Power", true));
    layout.add (std::make_unique<juce::AudioParameterChoice> (ParameterID { ids::airMode, 1 }, "Air Mode",
                    juce::StringArray { "Clean", "Bright", "Vintage", "Baby Boost" }, 0));
    layout.add (std::make_unique<juce::AudioParameterFloat> (ParameterID { ids::midHigh, 1 }, "Mid High",
                    juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f,
                    juce::AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction (percentText).withValueFromStringFunction (percentParse)));
    layout.add (std::make_unique<juce::AudioParameterFloat> (ParameterID { ids::high, 1 }, "High",
                    juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f,
                    juce::AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction (percentText).withValueFromStringFunction (percentParse)));
    layout.add (std::make_unique<juce::AudioParameterBool> (ParameterID { ids::link, 1 }, "Link", false));
    layout.add (std::make_unique<juce::AudioParameterFloat> (ParameterID { ids::trim, 1 }, "Trim",
                    juce::NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f,
                    juce::AudioParameterFloatAttributes().withLabel ("dB")
                        .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + " dB"; })
                        .withValueFromStringFunction ([] (const juce::String& t) { return t.retainCharacters ("0123456789.-").getFloatValue(); })));
    layout.add (std::make_unique<juce::AudioParameterFloat> (ParameterID { ids::mix, 1 }, "Mix",
                    juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
                    juce::AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v)) + "%"; })
                        .withValueFromStringFunction (percentParse)));
    layout.add (std::make_unique<juce::AudioParameterChoice> (ParameterID { ids::quality, 1 }, "Oversampling",
                    juce::StringArray { "2x", "4x", "8x", "16x" }, 2));
    return layout;
}
} // namespace babyair
