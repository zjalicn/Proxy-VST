#include "SampleLibrary.h"

SampleLibrary::SampleLibrary()
{
    formatManager.registerBasicFormats();
}

SampleLibrary::~SampleLibrary()
{
    clear();
}

bool SampleLibrary::loadFromFile(const juce::String &name, const juce::File &file)
{
    if (!file.existsAsFile())
        return false;

    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));

    if (reader == nullptr)
        return false;

    auto numChannels = static_cast<int>(reader->numChannels);
    auto lengthInSamples = static_cast<int>(reader->lengthInSamples);

    if (numChannels == 0 || lengthInSamples == 0)
        return false;

    SampleData newSample;
    newSample.buffer = std::make_unique<juce::AudioBuffer<float>>(numChannels, lengthInSamples);
    newSample.sampleRate = reader->sampleRate;
    newSample.maxLength = lengthInSamples;
    newSample.name = name;

    reader->read(newSample.buffer.get(), 0, lengthInSamples, 0, true, true);

    samples[name] = std::move(newSample);
    return true;
}

bool SampleLibrary::loadFromStream(const juce::String &name, juce::InputStream &stream)
{
    // Read the data from the stream into a MemoryBlock
    juce::MemoryBlock memoryBlock;
    stream.readIntoMemoryBlock(memoryBlock);

    // Create a MemoryInputStream from the MemoryBlock
    auto memoryInputStream = std::make_unique<juce::MemoryInputStream>(memoryBlock.getData(), memoryBlock.getSize(), false);

    // Create the format reader - pass the unique_ptr directly
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(std::move(memoryInputStream)));

    if (reader == nullptr)
        return false;

    auto numChannels = static_cast<int>(reader->numChannels);
    auto lengthInSamples = static_cast<int>(reader->lengthInSamples);

    if (numChannels == 0 || lengthInSamples == 0)
        return false;

    SampleData newSample;
    newSample.buffer = std::make_unique<juce::AudioBuffer<float>>(numChannels, lengthInSamples);
    newSample.sampleRate = reader->sampleRate;
    newSample.maxLength = lengthInSamples;
    newSample.name = name;

    reader->read(newSample.buffer.get(), 0, lengthInSamples, 0, true, true);

    samples[name] = std::move(newSample);
    return true;
}

bool SampleLibrary::loadFromBuffer(const juce::String &name, const juce::AudioBuffer<float> &buffer, double sampleRate)
{
    if (buffer.getNumChannels() == 0 || buffer.getNumSamples() == 0)
        return false;

    SampleData newSample;
    newSample.buffer = std::make_unique<juce::AudioBuffer<float>>(buffer.getNumChannels(), buffer.getNumSamples());
    newSample.buffer->makeCopyOf(buffer);
    newSample.sampleRate = sampleRate;
    newSample.maxLength = buffer.getNumSamples();
    newSample.name = name;

    samples[name] = std::move(newSample);
    return true;
}

SampleData SampleLibrary::getSampleAudioBuffer(const juce::String &name) const
{
    auto it = samples.find(name);

    if (it != samples.end())
    {
        // Create a new SampleData object and return it
        SampleData result;

        // Deep copy the audio buffer if it exists
        if (it->second.buffer)
        {
            result.buffer = std::make_unique<juce::AudioBuffer<float>>(
                it->second.buffer->getNumChannels(),
                it->second.buffer->getNumSamples());
            result.buffer->makeCopyOf(*(it->second.buffer));
        }

        // Copy other properties
        result.sampleRate = it->second.sampleRate;
        result.maxLength = it->second.maxLength;
        result.name = it->second.name;

        return result;
    }

    return SampleData(); // Return empty sample data if not found
}

juce::StringArray SampleLibrary::getAvailableSamples() const
{
    juce::StringArray result;

    for (const auto &pair : samples)
        result.add(pair.first);

    return result;
}

bool SampleLibrary::containsSample(const juce::String &name) const
{
    return samples.find(name) != samples.end();
}

void SampleLibrary::clear()
{
    samples.clear();
}

bool SampleLibrary::removeSample(const juce::String &name)
{
    auto it = samples.find(name);

    if (it != samples.end())
    {
        samples.erase(it);
        return true;
    }

    return false;
}