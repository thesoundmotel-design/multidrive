#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout MotelVoxAudioProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    // NOTE: the ranges below are design choices. The original ranges are not stored in the DLLs.
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "compression", 1 }, "Compression",
        NormalisableRange<float> (-30.0f, 0.0f, 0.1f), -10.0f,
        AudioParameterFloatAttributes().withLabel ("dB")));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "gate", 1 }, "Gate",
        NormalisableRange<float> (-79.0f, -20.0f, 0.1f), -79.0f,
        AudioParameterFloatAttributes().withLabel ("dB")
            .withStringFromValueFunction ([] (float v, int) { return v <= -78.95f ? String ("Off") : String (v, 1) + " dB"; })));

    NormalisableRange<float> rel (10.0f, 1000.0f, 1.0f);
    rel.setSkewForCentre (150.0f);
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "release", 1 }, "Release",
        rel, 150.0f, AudioParameterFloatAttributes().withLabel ("ms")));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "gain", 1 }, "Gain",
        NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("dB")));

    return { p.begin(), p.end() };
}

MotelVoxAudioProcessor::MotelVoxAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    pComp    = apvts.getRawParameterValue ("compression");
    pGate    = apvts.getRawParameterValue ("gate");
    pRelease = apvts.getRawParameterValue ("release");
    pGain    = apvts.getRawParameterValue ("gain");
}

bool MotelVoxAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    return in == out && (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo());
}

void MotelVoxAudioProcessor::prepareToPlay (double sampleRate, int)
{
    compressor.prepare (sampleRate, juce::jlimit (1, 2, getTotalNumInputChannels()));
    setLatencySamples (compressor.latencySamples());
}

void MotelVoxAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    motelvox::Settings s;
    s.compressionDb = pComp->load();
    s.gateDb        = pGate->load();
    s.releaseMs     = pRelease->load();
    s.gainDb        = pGain->load();
    compressor.setSettings (s);

    compressor.process (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples());
    grDb.store (compressor.lastGainReductionDb());
}

juce::AudioProcessorEditor* MotelVoxAudioProcessor::createEditor() { return new MotelVoxEditor (*this); }

void MotelVoxAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void MotelVoxAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MotelVoxAudioProcessor(); }
