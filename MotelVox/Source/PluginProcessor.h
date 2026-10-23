#pragma once
#include <atomic>
#include <juce_audio_processors/juce_audio_processors.h>
#include "MotelVoxDSP.h"

class MotelVoxAudioProcessor : public juce::AudioProcessor
{
public:
    MotelVoxAudioProcessor();
    ~MotelVoxAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Motel Vox"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> grDb { 0.0f };   // gain reduction for the meter

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    motelvox::Compressor compressor;
    std::atomic<float>* pComp = nullptr;
    std::atomic<float>* pGate = nullptr;
    std::atomic<float>* pRelease = nullptr;
    std::atomic<float>* pGain = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MotelVoxAudioProcessor)
};
