#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

// Flat motel-sign look: dark knob, orange rim, yellow pointer.
class MotelLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
};

class MotelVoxEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit MotelVoxEditor (MotelVoxAudioProcessor&);
    ~MotelVoxEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void setupKnob (juce::Slider&, juce::Label&, const juce::String& name);

    MotelVoxAudioProcessor& proc;
    MotelLookAndFeel laf;
    juce::Image logo;

    juce::Slider comp, gate, release, gain;
    juce::Label compL, gateL, releaseL, gainL;
    using Attach = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<Attach> compA, gateA, releaseA, gainA;

    float shownGr = 0.0f;
    juce::Rectangle<int> meterArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MotelVoxEditor)
};
