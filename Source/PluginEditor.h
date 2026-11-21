#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

//==============================================================================
// Draws knurled boutique-pedal knobs.
class PedalLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PedalLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle,
                           juce::Slider&) override;
};

//==============================================================================
// Chrome stomp switch.
class Footswitch : public juce::Button
{
public:
    Footswitch() : juce::Button ("Footswitch") { setClickingTogglesState (true); }

    void paintButton (juce::Graphics&, bool isHighlighted, bool isDown) override;
};

//==============================================================================
class UndertowAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     private juce::Timer
{
public:
    explicit UndertowAudioProcessorEditor (UndertowAudioProcessor&);
    ~UndertowAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void drawEnclosure (juce::Graphics&, juce::Rectangle<float>);
    void drawArtwork (juce::Graphics&, juce::Rectangle<float>);
    void drawLed (juce::Graphics&);

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Knob
    {
        juce::Slider slider;
        juce::String label;
        std::unique_ptr<SliderAttachment> attachment;
    };

    UndertowAudioProcessor& audioProcessor;
    PedalLookAndFeel lookAndFeel;

    std::array<Knob, 6> knobs;
    Footswitch footswitch;
    std::unique_ptr<ButtonAttachment> footswitchAttachment;

    juce::Rectangle<float> ledBounds;
    bool ledOn = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UndertowAudioProcessorEditor)
};
