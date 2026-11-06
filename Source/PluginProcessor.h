#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "DSP/ExciterEngine.h"
#include "DSP/MeteringEngine.h"
#include "Parameters.h"

class BabyAirProcessor : public juce::AudioProcessor, private juce::AsyncUpdater
{
public:
    BabyAirProcessor();
    ~BabyAirProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "BABY AIR"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram_; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    babyair::MeteringEngine inputMeter, outputMeter;

private:
    void handleAsyncUpdate() override { setLatencySamples (engine_.getLatencySamples()); }
    babyair::ExciterParams readParams() const;
    void setParamPlain (const char* id, float plainValue);

    babyair::ExciterEngine engine_;
    std::atomic<float>* pPower_ = nullptr; std::atomic<float>* pMode_ = nullptr;
    std::atomic<float>* pMid_ = nullptr;   std::atomic<float>* pHigh_ = nullptr;
    std::atomic<float>* pTrim_ = nullptr;  std::atomic<float>* pMix_ = nullptr;
    std::atomic<float>* pQuality_ = nullptr;
    int lastOsLog2_ = -1, currentProgram_ = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BabyAirProcessor)
};
