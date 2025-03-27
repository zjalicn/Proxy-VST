#pragma once

#include <JuceHeader.h>
#include <unordered_map>

// Structure to store sample data and its properties
struct SampleData
{
    std::unique_ptr<juce::AudioBuffer<float>> buffer;
    double sampleRate;
    int maxLength;
    juce::String name;

    SampleData() : buffer(nullptr), sampleRate(0.0), maxLength(0) {}
};

class SampleLibrary
{
public:
    SampleLibrary();
    ~SampleLibrary();

    // Load samples from various sources
    bool loadFromFile(const juce::String &name, const juce::File &file);
    bool loadFromStream(const juce::String &name, juce::InputStream &stream);
    bool loadFromBuffer(const juce::String &name, const juce::AudioBuffer<float> &buffer, double sampleRate);

    // Access samples
    SampleData getSampleAudioBuffer(const juce::String &name) const;
    juce::StringArray getAvailableSamples() const;
    bool containsSample(const juce::String &name) const;

    // Management
    void clear();
    bool removeSample(const juce::String &name);

private:
    std::unordered_map<juce::String, SampleData> samples;
    juce::AudioFormatManager formatManager;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SampleLibrary)
};