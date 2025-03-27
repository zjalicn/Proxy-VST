#include "PluginProcessor.h"
#include "PluginEditor.h"

ProxyAudioProcessor::ProxyAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      attackTimeMs(5.0f),
      releaseTimeMs(100.0f),
      gainValue(1.0f)
{
    // Initialize level meters
    levelLeft.reset(getSampleRate(), 0.1); // Smooth over 100ms
    levelRight.reset(getSampleRate(), 0.1);

    // Set initial parameters to the sampler
    samplerProcessor.setAttack(attackTimeMs);
    samplerProcessor.setRelease(releaseTimeMs);
    samplerProcessor.setGain(gainValue);
}

ProxyAudioProcessor::~ProxyAudioProcessor()
{
}

const juce::String ProxyAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool ProxyAudioProcessor::acceptsMidi() const
{
    return true;
}

bool ProxyAudioProcessor::producesMidi() const
{
    return false;
}

bool ProxyAudioProcessor::isMidiEffect() const
{
    return false;
}

double ProxyAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int ProxyAudioProcessor::getNumPrograms()
{
    return 1; // NB: some hosts don't cope very well if you tell them there are 0 programs,
              // so this should be at least 1, even if you're not really implementing programs.
}

int ProxyAudioProcessor::getCurrentProgram()
{
    return 0;
}

void ProxyAudioProcessor::setCurrentProgram(int index)
{
    // We only have one program, so do nothing
}

const juce::String ProxyAudioProcessor::getProgramName(int index)
{
    return "Default";
}

void ProxyAudioProcessor::changeProgramName(int index, const juce::String &newName)
{
    // We don't allow program name changes
}

void ProxyAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // Initialize level meters with the correct sample rate
    levelLeft.reset(sampleRate, 0.1);
    levelRight.reset(sampleRate, 0.1);

    // Prepare sampler processor
    samplerProcessor.prepareToPlay(sampleRate, samplesPerBlock);
}

void ProxyAudioProcessor::releaseResources()
{
    samplerProcessor.releaseResources();
}

bool ProxyAudioProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const
{
    // We support mono or stereo output
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono() && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

void ProxyAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear any output channels that didn't contain input data
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // Process through our sampler
    samplerProcessor.processBlock(buffer, midiMessages);

    // Update level meters
    if (totalNumOutputChannels > 0)
    {
        levelLeft.setTargetValue(buffer.getRMSLevel(0, 0, buffer.getNumSamples()));
        levelLeft.skip(buffer.getNumSamples());
    }

    if (totalNumOutputChannels > 1)
    {
        levelRight.setTargetValue(buffer.getRMSLevel(1, 0, buffer.getNumSamples()));
        levelRight.skip(buffer.getNumSamples());
    }
}

bool ProxyAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor *ProxyAudioProcessor::createEditor()
{
    return new ProxyAudioProcessorEditor(*this);
}

void ProxyAudioProcessor::getStateInformation(juce::MemoryBlock &destData)
{
    // Save the processor state
    juce::MemoryOutputStream stream(destData, true);

    // Save the sampler parameters
    stream.writeFloat(samplerProcessor.getAttack());
    stream.writeFloat(samplerProcessor.getRelease());
    stream.writeFloat(samplerProcessor.getGain());

    // Save currently selected sample name
    stream.writeString(samplerProcessor.getCurrentSampleName());
}

void ProxyAudioProcessor::setStateInformation(const void *data, int sizeInBytes)
{
    // Restore processor state
    juce::MemoryInputStream stream(data, static_cast<size_t>(sizeInBytes), false);

    // Check how much data we have available
    const int bytesAvailable = stream.getNumBytesRemaining();

    if (bytesAvailable >= sizeof(float) * 3 + sizeof(int))
    {
        // Load basic parameters
        float attack = stream.readFloat();
        float release = stream.readFloat();
        float gain = stream.readFloat();

        // Set the parameters to the sampler
        samplerProcessor.setAttack(attack);
        samplerProcessor.setRelease(release);
        samplerProcessor.setGain(gain);

        // Load sample name
        juce::String sampleName = stream.readString();
        if (sampleName.isNotEmpty())
        {
            samplerProcessor.setSample(sampleName);
        }
    }
}

// This creates new instances of the plugin
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new ProxyAudioProcessor();
}