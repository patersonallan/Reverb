#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    juce::String percentText (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; }
    juce::String secondsText (float v, int) { return juce::String (v, v < 10.0f ? 1 : 0) + " s"; }

    std::unique_ptr<juce::AudioParameterFloat> makePercent (const char* id, const char* name, float def)
    {
        return std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 }, name,
            juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), def,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction (percentText));
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout UndertowAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (makePercent (ParamIDs::mix, "Mix", 0.4f));

    juce::NormalisableRange<float> lengthRange (0.3f, 15.0f, 0.01f);
    lengthRange.setSkewForCentre (3.0f);
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::length, 1 }, "Length", lengthRange, 3.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (secondsText)));

    layout.add (makePercent (ParamIDs::diffuse, "Diffuse", 0.6f));
    layout.add (makePercent (ParamIDs::dampen,  "Dampen",  0.4f));
    layout.add (makePercent (ParamIDs::drag,    "Drag",    0.5f));
    layout.add (makePercent (ParamIDs::reflect, "Reflect", 0.2f));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParamIDs::engaged, 1 }, "Engaged", true));

    return layout;
}

UndertowAudioProcessor::UndertowAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "UndertowState", createParameterLayout())
{
    mixParam     = apvts.getRawParameterValue (ParamIDs::mix);
    lengthParam  = apvts.getRawParameterValue (ParamIDs::length);
    diffuseParam = apvts.getRawParameterValue (ParamIDs::diffuse);
    dampenParam  = apvts.getRawParameterValue (ParamIDs::dampen);
    dragParam    = apvts.getRawParameterValue (ParamIDs::drag);
    reflectParam = apvts.getRawParameterValue (ParamIDs::reflect);
    engagedParam = apvts.getRawParameterValue (ParamIDs::engaged);
}

void UndertowAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    reverb.prepare (sampleRate);
    monoScratch.setSize (1, juce::jmax (1, samplesPerBlock), false, true, false);
}

bool UndertowAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    if (in != juce::AudioChannelSet::mono() && in != juce::AudioChannelSet::stereo())
        return false;

    // mono->mono, mono->stereo, stereo->stereo
    return in.size() <= out.size();
}

void UndertowAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, numSamples);

    undertow::SwarmParams p;
    p.mix     = mixParam->load();
    p.length  = lengthParam->load();
    p.diffuse = diffuseParam->load();
    p.dampen  = dampenParam->load();
    p.drag    = dragParam->load();
    p.reflect = reflectParam->load();
    p.engaged = engagedParam->load() > 0.5f;
    reverb.setParams (p);

    if (buffer.getNumChannels() >= 2)
    {
        if (numIn == 1)
            buffer.copyFrom (1, 0, buffer, 0, 0, numSamples); // mono guitar -> stereo reverb

        reverb.process (buffer.getWritePointer (0), buffer.getWritePointer (1), numSamples);
    }
    else if (buffer.getNumChannels() == 1)
    {
        if (monoScratch.getNumSamples() < numSamples)
            monoScratch.setSize (1, numSamples, false, false, true);

        auto* l = buffer.getWritePointer (0);
        auto* r = monoScratch.getWritePointer (0);
        juce::FloatVectorOperations::copy (r, l, numSamples);
        reverb.process (l, r, numSamples);
        juce::FloatVectorOperations::add (l, r, numSamples);
        juce::FloatVectorOperations::multiply (l, 0.5f, numSamples);
    }
}

juce::AudioProcessorEditor* UndertowAudioProcessor::createEditor()
{
    return new UndertowAudioProcessorEditor (*this);
}

void UndertowAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void UndertowAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new UndertowAudioProcessor();
}
