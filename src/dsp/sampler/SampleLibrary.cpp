#include "SampleLibrary.h"

SampleLibrary::SampleLibrary()
{
    formatManager.registerBasicFormats();
}

SampleLibrary::~SampleLibrary()
{
    clear();
}

bool SampleLibrary::loadFromFile(const juce::String &name, const juce::File &file, const juce::String &category)
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
    newSample.category = category;

    reader->read(newSample.buffer.get(), 0, lengthInSamples, 0, true, true);

    // Store the sample in the main samples map
    samples[name] = std::move(newSample);

    // Add to category list
    if (category.isNotEmpty())
    {
        categories[category].addIfNotAlreadyThere(name);
    }
    else
    {
        // Use "Uncategorized" for samples without a category
        categories["Uncategorized"].addIfNotAlreadyThere(name);
    }

    return true;
}

bool SampleLibrary::loadFromStream(const juce::String &name, juce::InputStream &stream, const juce::String &category)
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
    newSample.category = category;

    reader->read(newSample.buffer.get(), 0, lengthInSamples, 0, true, true);

    // Store the sample in the main samples map
    samples[name] = std::move(newSample);

    // Add to category list
    if (category.isNotEmpty())
    {
        categories[category].addIfNotAlreadyThere(name);
    }
    else
    {
        // Use "Uncategorized" for samples without a category
        categories["Uncategorized"].addIfNotAlreadyThere(name);
    }

    return true;
}

bool SampleLibrary::loadFromBuffer(const juce::String &name, const juce::AudioBuffer<float> &buffer, double sampleRate, const juce::String &category)
{
    if (buffer.getNumChannels() == 0 || buffer.getNumSamples() == 0)
        return false;

    SampleData newSample;
    newSample.buffer = std::make_unique<juce::AudioBuffer<float>>(buffer.getNumChannels(), buffer.getNumSamples());
    newSample.buffer->makeCopyOf(buffer);
    newSample.sampleRate = sampleRate;
    newSample.maxLength = buffer.getNumSamples();
    newSample.name = name;
    newSample.category = category;

    // Store the sample in the main samples map
    samples[name] = std::move(newSample);

    // Add to category list
    if (category.isNotEmpty())
    {
        categories[category].addIfNotAlreadyThere(name);
    }
    else
    {
        // Use "Uncategorized" for samples without a category
        categories["Uncategorized"].addIfNotAlreadyThere(name);
    }

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
        result.category = it->second.category;

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

juce::StringArray SampleLibrary::getSamplesInCategory(const juce::String &category) const
{
    auto it = categories.find(category);
    if (it != categories.end())
    {
        return it->second;
    }

    return juce::StringArray(); // Return empty array if category not found
}

juce::StringArray SampleLibrary::getCategories() const
{
    juce::StringArray result;

    for (const auto &pair : categories)
    {
        // Only add categories that have samples
        if (pair.second.size() > 0)
        {
            result.add(pair.first);
        }
    }

    return result;
}

juce::String SampleLibrary::getSampleCategory(const juce::String &name) const
{
    auto it = samples.find(name);
    if (it != samples.end())
    {
        return it->second.category;
    }

    return ""; // Return empty string if sample not found
}

bool SampleLibrary::containsSample(const juce::String &name) const
{
    return samples.find(name) != samples.end();
}

juce::File SampleLibrary::getSamplesFolder() const
{
    // Get user documents folder
    juce::File docsFolder = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
    juce::File samplesFolder = docsFolder.getChildFile("Proxy/Samples");

    // Create the directory if it doesn't exist
    if (!samplesFolder.exists())
    {
        juce::Result result = samplesFolder.createDirectory();
        if (result.failed())
        {
            juce::Logger::writeToLog("Failed to create samples directory: " + result.getErrorMessage());
        }
    }

    return samplesFolder;
}

bool SampleLibrary::scanFolderForSamples(const juce::File &folder, const juce::String &category)
{
    if (!folder.exists() || !folder.isDirectory())
        return false;

    // Get all audio files in the directory
    juce::Array<juce::File> audioFiles;
    const int maxFilesToScan = 200; // Reasonable limit to prevent excessive scanning

    // Use a more complete wildcard pattern and explicitly set recursive flag to false
    folder.findChildFiles(audioFiles, juce::File::findFiles, false, "*.wav;*.aif;*.aiff;*.mp3");

    // Limit the number of files for performance
    if (audioFiles.size() > maxFilesToScan)
        audioFiles.resize(maxFilesToScan);

    int filesLoaded = 0;

    // Use the folder name as category if no category provided
    juce::String effectiveCategory = category.isNotEmpty() ? category : folder.getFileName();

    // Load each audio file
    for (const auto &file : audioFiles)
    {
        // Get the name without extension (properly handles spaces in filenames)
        juce::String name = file.getFileNameWithoutExtension();

        // Skip if we already have a sample with this name
        if (containsSample(name))
            continue;

        if (loadFromFile(name, file, effectiveCategory))
            filesLoaded++;
    }

    return filesLoaded > 0;
}

bool SampleLibrary::scanFolderAndSubfoldersForSamples(const juce::File &rootFolder)
{
    if (!rootFolder.exists() || !rootFolder.isDirectory())
        return false;

    bool anyFilesLoaded = false;

    // Scan the main folder (using root folder name as category)
    anyFilesLoaded |= scanFolderForSamples(rootFolder, "");

    // Get all subdirectories
    juce::Array<juce::File> subdirectories;
    rootFolder.findChildFiles(subdirectories, juce::File::findDirectories, false);

    // Scan each subdirectory (using subdirectory name as category)
    for (const auto &dir : subdirectories)
    {
        anyFilesLoaded |= scanFolderForSamples(dir, dir.getFileName());
    }

    return anyFilesLoaded;
}

juce::StringArray SampleLibrary::scanUserSamplesFolder()
{
    // Clear existing samples
    clear();

    // Get the samples folder
    juce::File samplesFolder = getSamplesFolder();

    // Scan the folder and its subfolders for samples
    scanFolderAndSubfoldersForSamples(samplesFolder);

    // Return the list of available samples
    return getAvailableSamples();
}

void SampleLibrary::clear()
{
    samples.clear();
    categories.clear();
}

bool SampleLibrary::removeSample(const juce::String &name)
{
    auto it = samples.find(name);

    if (it != samples.end())
    {
        // Remove from category
        juce::String category = it->second.category;
        if (category.isNotEmpty())
        {
            auto categoryIt = categories.find(category);
            if (categoryIt != categories.end())
            {
                categoryIt->second.removeString(name);
            }
        }

        // Remove from samples map
        samples.erase(it);
        return true;
    }

    return false;
}