#pragma once

#include <JuceHeader.h>
#include "SamplerProcessor.h"

class ProxyAudioProcessor : public juce::AudioProcessor
{
public:
    ProxyAudioProcessor();
    ~ProxyAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout &layouts) const override;

    void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;

    juce::AudioProcessorEditor *createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String &newName) override;

    void getStateInformation(juce::MemoryBlock &destData) override;
    void setStateInformation(const void *data, int sizeInBytes) override;

    // Access to the sampler processor
    SamplerProcessor &getSamplerProcessor() { return samplerProcessor; }

    // Audio level metering
    float getLeftLevel() const { return levelLeft.getCurrentValue(); }
    float getRightLevel() const { return levelRight.getCurrentValue(); }

private:
    SamplerProcessor samplerProcessor;

    // Parameter values
    float attackTimeMs;
    float releaseTimeMs;
    float gainValue;
    bool monophonic;

    // Level metering
    juce::LinearSmoothedValue<float> levelLeft, levelRight;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProxyAudioProcessor)
};