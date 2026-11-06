#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets/FactoryPresets.h"

BabyAirProcessor::BabyAirProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "BABY_AIR_STATE", babyair::createParameterLayout())
{
    using namespace babyair::ids;
    pPower_ = apvts.getRawParameterValue (power);   pMode_ = apvts.getRawParameterValue (airMode);
    pMid_ = apvts.getRawParameterValue (midHigh);   pHigh_ = apvts.getRawParameterValue (high);
    pTrim_ = apvts.getRawParameterValue (trim);     pMix_ = apvts.getRawParameterValue (mix);
    pQuality_ = apvts.getRawParameterValue (quality);
}

bool BabyAirProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto& in = l.getMainInputChannelSet();
    return in == l.getMainOutputChannelSet() && (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo());
}

babyair::ExciterParams BabyAirProcessor::readParams() const
{
    babyair::ExciterParams p;
    p.power   = pPower_->load() > 0.5f;
    p.mode    = juce::jlimit (0, 3, (int) std::lround (pMode_->load()));
    p.midHigh = pMid_->load() * 0.01f;
    p.high    = pHigh_->load() * 0.01f;
    p.trimDb  = pTrim_->load();
    p.mix     = pMix_->load() * 0.01f;
    p.osLog2  = juce::jlimit (1, 4, (int) std::lround (pQuality_->load()) + 1);   // choice 0..3 -> 2x..16x
    return p;
}

void BabyAirProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const auto p = readParams();
    engine_.setParams (p);                      // set first so prepare() snaps smoothers to the real values
    engine_.prepare (sampleRate, samplesPerBlock, juce::jmax (1, getTotalNumInputChannels()));
    lastOsLog2_ = p.osLog2;
    inputMeter.prepare (sampleRate);
    outputMeter.prepare (sampleRate);
    setLatencySamples (engine_.getLatencySamples());
}

void BabyAirProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int nCh = juce::jmin (buffer.getNumChannels(), babyair::kMaxChannels);
    float* const* data = buffer.getArrayOfWritePointers();

    inputMeter.process (data, nCh, n);

    const auto p = readParams();
    engine_.setParams (p);
    if (p.osLog2 != lastOsLog2_)                // quality changed: latency changed -> tell the host (message thread)
    {
        lastOsLog2_ = p.osLog2;
        triggerAsyncUpdate();
    }
    engine_.process (data, nCh, n);

    outputMeter.process (data, nCh, n);
}

juce::AudioProcessorEditor* BabyAirProcessor::createEditor() { return new BabyAirEditor (*this); }

int BabyAirProcessor::getNumPrograms() { return babyair::kNumFactoryPresets; }
const juce::String BabyAirProcessor::getProgramName (int i)
{
    return (i >= 0 && i < babyair::kNumFactoryPresets) ? babyair::kFactoryPresets[i].name : "";
}

void BabyAirProcessor::setParamPlain (const char* id, float v)
{
    if (auto* prm = apvts.getParameter (id))
        prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

void BabyAirProcessor::setCurrentProgram (int index)
{
    if (index < 0 || index >= babyair::kNumFactoryPresets) return;
    currentProgram_ = index;
    const auto& pr = babyair::kFactoryPresets[index];
    setParamPlain (babyair::ids::airMode, (float) pr.mode);
    setParamPlain (babyair::ids::link, 0.0f);   // avoid link side effects while loading both values
    setParamPlain (babyair::ids::midHigh, pr.midHigh);
    setParamPlain (babyair::ids::high, pr.high);
    setParamPlain (babyair::ids::trim, pr.trimDb);
    setParamPlain (babyair::ids::mix, pr.mix);
    setParamPlain (babyair::ids::link, pr.link ? 1.0f : 0.0f);
}

void BabyAirProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram_, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void BabyAirProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            currentProgram_ = (int) state.getProperty ("program", 0);
            apvts.replaceState (state);
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new BabyAirProcessor(); }
