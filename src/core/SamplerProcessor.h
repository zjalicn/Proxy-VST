#pragma once

#include <JuceHeader.h>
#include "SampleLibrary.h"

class SamplerProcessor
{
public:
    SamplerProcessor();
    ~SamplerProcessor();

    // Sample management
    void loadSample(const juce::File &file);
    void loadDefaultSamples();
    bool setSample(const juce::String &name);
    juce::StringArray getAvailableSamples() const;
    juce::String getCurrentSampleName() const;

    // AudioProcessor methods
    void prepareToPlay(double sampleRate, int samplesPerBlock);
    void processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages);
    void reset();
    void releaseResources();

    // MIDI and voice handling
    void handleMidiEvent(const juce::MidiMessage &midiMessage);
    void setCurrentPlaybackSamplePosition(int pos) { currentSamplePosition = pos; }
    int getCurrentPlaybackSamplePosition() const { return currentSamplePosition; }

    // Parameters
    void setAttack(float attackTimeMs);
    void setRelease(float releaseTimeMs);
    void setGain(float newGain);

    float getAttack() const { return attackTimeMs; }
    float getRelease() const { return releaseTimeMs; }
    float getGain() const { return gain; }

    // Get access to the sample library
    const SampleLibrary &getSampleLibrary() const { return sampleLibrary; }

private:
    // Sample managers
    SampleLibrary sampleLibrary;
    std::unique_ptr<juce::Synthesiser> sampler;

    // Current state
    juce::String currentSampleName;
    int currentSamplePosition;

    // Parameters
    float attackTimeMs;
    float releaseTimeMs;
    float gain;

    // Voice management
    void updateVoiceParameters();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SamplerProcessor)
};