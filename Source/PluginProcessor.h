#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "SwarmReverb.h"

namespace ParamIDs
{
    inline constexpr const char* mix     = "mix";
    inline constexpr const char* length  = "length";
    inline constexpr const char* diffuse = "diffuse";
    inline constexpr const char* dampen  = "dampen";
    inline constexpr const char* drag    = "drag";
    inline constexpr const char* reflect = "reflect";
    inline constexpr const char* engaged = "engaged";
}

class UndertowAudioProcessor : public juce::AudioProcessor
{
public:
    UndertowAudioProcessor();
    ~UndertowAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 15.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState apvts;

private:
    undertow::SwarmReverb reverb;
    juce::AudioBuffer<float> monoScratch;

    std::atomic<float>* mixParam     = nullptr;
    std::atomic<float>* lengthParam  = nullptr;
    std::atomic<float>* diffuseParam = nullptr;
    std::atomic<float>* dampenParam  = nullptr;
    std::atomic<float>* dragParam    = nullptr;
    std::atomic<float>* reflectParam = nullptr;
    std::atomic<float>* engagedParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UndertowAudioProcessor)
};
