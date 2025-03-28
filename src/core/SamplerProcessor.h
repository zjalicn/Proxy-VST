#pragma once

#include <JuceHeader.h>
#include "SampleLibrary.h"

// Structure to store voice playback positions
struct VoicePosition
{
    int position;
    bool isActive;

    VoicePosition() : position(0), isActive(false) {}
};

class SamplerProcessor
{
public:
    static constexpr int MAX_VOICES = 8;

    SamplerProcessor();
    ~SamplerProcessor();

    // Sample management
    void loadSample(const juce::File &file);
    void loadDefaultSamples();
    bool setSample(const juce::String &name);
    juce::StringArray getAvailableSamples() const;
    juce::String getCurrentSampleName() const;

    // Refresh samples from folder
    void refreshSamples();

    // AudioProcessor methods
    void prepareToPlay(double sampleRate, int samplesPerBlock);
    void processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages);
    void reset();
    void releaseResources();

    // MIDI and voice handling
    void handleMidiEvent(const juce::MidiMessage &midiMessage);
    void setCurrentPlaybackSamplePosition(int pos) { currentSamplePosition = pos; }
    int getCurrentPlaybackSamplePosition() const { return currentSamplePosition; }
    bool isAnyVoiceActive() const;

    // Get all voice positions
    const std::array<VoicePosition, MAX_VOICES> &getAllVoicePositions() const { return voicePositions; }

    // Parameters
    void setAttack(float attackTimeMs);
    void setRelease(float releaseTimeMs);
    void setGain(float newGain);
    void setMonophonic(bool isMonophonic);
    void updateActiveVoices();

    float getAttack() const { return attackTimeMs; }
    float getRelease() const { return releaseTimeMs; }
    float getGain() const { return gain; }
    bool isMonophonic() const { return monophonic; }

    // Get access to the sample library
    const SampleLibrary &getSampleLibrary() const { return sampleLibrary; }

private:
    // Sample managers
    SampleLibrary sampleLibrary;
    std::unique_ptr<juce::Synthesiser> sampler;

    // Current state
    juce::String currentSampleName;
    int currentSamplePosition;

    // Track positions for all voices
    std::array<VoicePosition, MAX_VOICES> voicePositions;

    // Parameters
    float attackTimeMs;
    float releaseTimeMs;
    float gain;
    bool monophonic;

    // Monophonic mode tracking
    int lastMonophonicNote;

    // Anti-pop buffer for monophonic mode
    juce::AudioBuffer<float> antiPopBuffer;
    bool antiPopCaptured = false;

    // Anti-pop methods
    void captureAntiPopBuffer(const juce::AudioBuffer<float> &buffer);
    void applyAntiPopProcessing(juce::AudioBuffer<float> &buffer);

    // Voice management
    void updateVoiceParameters();
    void updateVoicePositions();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SamplerProcessor)
};