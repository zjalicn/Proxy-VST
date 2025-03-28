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
    juce::String category; // Add category field to store folder name

    SampleData() : buffer(nullptr), sampleRate(0.0), maxLength(0) {}

    // Add move constructor
    SampleData(SampleData &&other) noexcept
        : buffer(std::move(other.buffer)),
          sampleRate(other.sampleRate),
          maxLength(other.maxLength),
          name(std::move(other.name)),
          category(std::move(other.category))
    {
    }

    // Add move assignment operator
    SampleData &operator=(SampleData &&other) noexcept
    {
        buffer = std::move(other.buffer);
        sampleRate = other.sampleRate;
        maxLength = other.maxLength;
        name = std::move(other.name);
        category = std::move(other.category);
        return *this;
    }

    // Delete copy constructor and assignment operator explicitly
    SampleData(const SampleData &) = delete;
    SampleData &operator=(const SampleData &) = delete;
};

// Structure to organize samples by category
struct CategoryData
{
    juce::String name;
    juce::StringArray samples;
};

class SampleLibrary
{
public:
    SampleLibrary();
    ~SampleLibrary();

    // Load samples from various sources
    bool loadFromFile(const juce::String &name, const juce::File &file, const juce::String &category = "");
    bool loadFromStream(const juce::String &name, juce::InputStream &stream, const juce::String &category = "");
    bool loadFromBuffer(const juce::String &name, const juce::AudioBuffer<float> &buffer, double sampleRate, const juce::String &category = "");

    // Access samples
    SampleData getSampleAudioBuffer(const juce::String &name) const;
    juce::StringArray getAvailableSamples() const;
    juce::StringArray getSamplesInCategory(const juce::String &category) const;
    juce::StringArray getCategories() const;
    bool containsSample(const juce::String &name) const;
    juce::String getSampleCategory(const juce::String &name) const;

    // Folder scanning and management
    bool scanFolderForSamples(const juce::File &folder, const juce::String &category = "");
    bool scanFolderAndSubfoldersForSamples(const juce::File &rootFolder);
    juce::File getSamplesFolder() const;
    juce::StringArray scanUserSamplesFolder();

    // Management
    void clear();
    bool removeSample(const juce::String &name);

private:
    std::unordered_map<juce::String, SampleData> samples;
    std::unordered_map<juce::String, juce::StringArray> categories; // Category name -> sample names
    juce::AudioFormatManager formatManager;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SampleLibrary)
};