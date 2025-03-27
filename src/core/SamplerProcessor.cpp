#include "SamplerProcessor.h"
#include <BinaryData.h>

// Forward declare our custom classes
class ProxySamplerSound;
class ProxySamplerVoice;

// Custom sampler sound that contains an audio buffer
class ProxySamplerSound : public juce::SynthesiserSound
{
public:
    ProxySamplerSound(const juce::String &soundName, const juce::AudioBuffer<float> &buffer)
        : name(soundName)
    {
        // Make a copy of the buffer
        audioData.setSize(buffer.getNumChannels(), buffer.getNumSamples());
        audioData.copyFrom(0, 0, buffer, 0, 0, buffer.getNumSamples());
        if (buffer.getNumChannels() > 1)
            audioData.copyFrom(1, 0, buffer, 1, 0, buffer.getNumSamples());
    }

    // SynthesiserSound interface implementation
    bool appliesToNote(int /*midiNoteNumber*/) override { return true; }
    bool appliesToChannel(int /*midiChannel*/) override { return true; }

    // Provide access to the audio data
    const juce::AudioBuffer<float> &getAudioData() const { return audioData; }

    const juce::String &getName() const { return name; }

private:
    juce::AudioBuffer<float> audioData;
    juce::String name;
};

// A custom sampler voice that plays a buffer
class ProxySamplerVoice : public juce::SynthesiserVoice
{
public:
    ProxySamplerVoice()
        : currentSamplePosition(0.0),
          pitchRatio(0.0),
          sourceSamplePosition(0.0),
          lgain(0.0f),
          rgain(0.0f),
          attackSamples(0.0),
          releaseSamples(0.0),
          attackRate(0.0),
          releaseRate(0.0),
          attackPhase(false),
          releasePhase(false),
          envelopeLevel(0.0)
    {
    }

    bool canPlaySound(juce::SynthesiserSound *sound) override
    {
        // Check if this is our custom sound class
        return dynamic_cast<ProxySamplerSound *>(sound) != nullptr;
    }

    void startNote(int midiNoteNumber, float velocity,
                   juce::SynthesiserSound *s, int /*currentPitchWheelPosition*/) override
    {
        if (auto *sound = dynamic_cast<ProxySamplerSound *>(s))
        {
            // Calculate pitch ratio based on the difference between the played MIDI note and middle C (60)
            pitchRatio = std::pow(2.0, (midiNoteNumber - 60) / 12.0);
            sourceSamplePosition = 0.0;
            lgain = velocity;
            rgain = velocity;

            // Reset envelope
            envelopeLevel = 0.0;
            attackPhase = true;
            releasePhase = false;

            currentSamplePosition = 0.0;
        }
        else
        {
            stopNote(0.0f, false);
        }
    }

    void stopNote(float /*velocity*/, bool allowTailOff) override
    {
        if (allowTailOff)
        {
            releasePhase = true;
            attackPhase = false;
        }
        else
        {
            clearCurrentNote();
        }
    }

    void setAttackRate(double sampleRate, double attackTimeMs)
    {
        attackSamples = sampleRate * (attackTimeMs / 1000.0);
        attackRate = attackSamples > 0 ? 1.0 / attackSamples : 1.0;
    }

    void setReleaseRate(double sampleRate, double releaseTimeMs)
    {
        releaseSamples = sampleRate * (releaseTimeMs / 1000.0);
        releaseRate = releaseSamples > 0 ? 1.0 / releaseSamples : 1.0;
    }

    void pitchWheelMoved(int /*newValue*/) override {}
    void controllerMoved(int /*controllerNumber*/, int /*newValue*/) override {}

    void renderNextBlock(juce::AudioBuffer<float> &outputBuffer, int startSample, int numSamples) override
    {
        if (auto *playingSound = dynamic_cast<ProxySamplerSound *>(getCurrentlyPlayingSound().get()))
        {
            const juce::AudioBuffer<float> &audioData = playingSound->getAudioData();
            const float *const inL = audioData.getReadPointer(0);
            const float *const inR = audioData.getNumChannels() > 1 ? audioData.getReadPointer(1) : nullptr;

            float *outL = outputBuffer.getWritePointer(0, startSample);
            float *outR = outputBuffer.getNumChannels() > 1 ? outputBuffer.getWritePointer(1, startSample) : nullptr;

            // Save the current position to a local variable
            double localSourceSamplePosition = this->sourceSamplePosition;

            while (--numSamples >= 0)
            {
                auto pos = (int)localSourceSamplePosition;
                auto alpha = (float)(localSourceSamplePosition - pos);
                auto invAlpha = 1.0f - alpha;

                // Simple linear interpolation
                int nextPos = pos + 1;
                if (nextPos >= audioData.getNumSamples())
                    nextPos = pos; // Avoid reading beyond the buffer

                // Update envelope
                if (attackPhase)
                {
                    envelopeLevel += attackRate;
                    if (envelopeLevel >= 1.0)
                    {
                        envelopeLevel = 1.0;
                        attackPhase = false;
                    }
                }
                else if (releasePhase)
                {
                    envelopeLevel -= releaseRate;
                    if (envelopeLevel <= 0.0)
                    {
                        clearCurrentNote();
                        break;
                    }
                }

                float l = (inL[pos] * invAlpha + inL[nextPos] * alpha) * (float)envelopeLevel;

                if (outR != nullptr)
                {
                    float r = inR != nullptr ? (inR[pos] * invAlpha + inR[nextPos] * alpha) * (float)envelopeLevel
                                             : l;

                    *outL++ += l * lgain;
                    *outR++ += r * rgain;
                }
                else
                {
                    *outL++ += l * lgain;
                }

                localSourceSamplePosition += pitchRatio;

                if (localSourceSamplePosition >= audioData.getNumSamples())
                {
                    // For one-shot samples, we just stop when we reach the end
                    if (!releasePhase)
                    {
                        releasePhase = true;
                    }

                    // Loop playback position for visualization
                    // This is crucial for seeing the full waveform
                    localSourceSamplePosition = 0.0;
                }

                // Update the current position for visualization
                currentSamplePosition = (pos * 1.0) / audioData.getNumSamples() * audioData.getNumSamples();
            }

            this->sourceSamplePosition = localSourceSamplePosition;
        }
    }

    bool isVoiceActive() const
    {
        return getCurrentlyPlayingSound() != nullptr && !releasePhase;
    }

    // Current sample position as a percentage of the total length
    double getCurrentSamplePosition() const
    {
        if (auto *sound = dynamic_cast<ProxySamplerSound *>(getCurrentlyPlayingSound().get()))
        {
            // Return current sample position as a proportion of the total sample length
            return (sourceSamplePosition / sound->getAudioData().getNumSamples()) * sound->getAudioData().getNumSamples();
        }
        return 0.0;
    }

private:
    double currentSamplePosition;
    double pitchRatio;
    double sourceSamplePosition;
    float lgain, rgain;

    // Envelope parameters
    double attackSamples, releaseSamples;
    double attackRate, releaseRate;
    bool attackPhase, releasePhase;
    double envelopeLevel;
};

SamplerProcessor::SamplerProcessor()
    : currentSamplePosition(0),
      attackTimeMs(5.0f),    // 5ms default attack
      releaseTimeMs(100.0f), // 100ms default release
      gain(1.0f)
{
    sampler = std::make_unique<juce::Synthesiser>();

    // Add voices to the sampler
    for (int i = 0; i < 16; ++i)
    {
        sampler->addVoice(new ProxySamplerVoice());
    }

    // Load the default sample
    loadDefaultSample();
}

SamplerProcessor::~SamplerProcessor()
{
}

void SamplerProcessor::loadDefaultSample()
{
    // Load the built-in 808 sample from binary data
    int size = 0;
    const void *data = BinaryData::getNamedResource("zay_808_wav", size);

    if (data != nullptr && size > 0)
    {
        juce::MemoryInputStream stream(data, static_cast<size_t>(size), false);
        sampleLibrary.loadFromStream("zay_808", stream);
        setSample("zay_808");
    }
}

void SamplerProcessor::loadSample(const juce::File &file)
{
    juce::String name = file.getFileNameWithoutExtension();
    sampleLibrary.loadFromFile(name, file);
    setSample(name);
}

bool SamplerProcessor::setSample(const juce::String &name)
{
    auto sampleData = sampleLibrary.getSampleAudioBuffer(name);

    if (sampleData.buffer && sampleData.buffer->getNumSamples() > 0)
    {
        sampler->clearSounds();

        // Create a new ProxySamplerSound with the buffer
        auto *sound = new ProxySamplerSound(name, *sampleData.buffer);

        sampler->addSound(sound);
        currentSampleName = name;
        updateVoiceParameters();
        return true;
    }

    return false;
}

juce::StringArray SamplerProcessor::getAvailableSamples() const
{
    return sampleLibrary.getAvailableSamples();
}

juce::String SamplerProcessor::getCurrentSampleName() const
{
    return currentSampleName;
}

void SamplerProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/)
{
    sampler->setCurrentPlaybackSampleRate(sampleRate);
    updateVoiceParameters();
}

void SamplerProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages)
{
    buffer.clear();

    // Process incoming MIDI messages
    for (const auto &metadata : midiMessages)
    {
        handleMidiEvent(metadata.getMessage());
    }

    // Render the sampler audio
    sampler->renderNextBlock(buffer, midiMessages, 0, buffer.getNumSamples());

    // Apply gain
    if (gain != 1.0f)
    {
        buffer.applyGain(gain);
    }

    // Update current playback position for display purposes
    // First, check if any voice is actually playing
    bool anyVoicePlaying = false;
    int updatedPosition = 0;

    for (int i = 0; i < sampler->getNumVoices(); ++i)
    {
        if (auto *voice = dynamic_cast<ProxySamplerVoice *>(sampler->getVoice(i)))
        {
            if (voice->isVoiceActive())
            {
                updatedPosition = static_cast<int>(voice->getCurrentSamplePosition());
                anyVoicePlaying = true;
                break; // Only need to find one active voice
            }
        }
    }

    if (anyVoicePlaying)
    {
        currentSamplePosition = updatedPosition;
    }
    else
    {
        // If no voices are playing, we can just keep the last position
        // or reset it to 0 depending on desired behavior
    }
}

void SamplerProcessor::reset()
{
    sampler->allNotesOff(0, true);
}

void SamplerProcessor::releaseResources()
{
    reset();
}

void SamplerProcessor::handleMidiEvent(const juce::MidiMessage &midiMessage)
{
    // Pass the MIDI message directly to the sampler
    if (sampler != nullptr)
    {
        if (midiMessage.isNoteOn())
        {
            sampler->noteOn(midiMessage.getChannel(),
                            midiMessage.getNoteNumber(),
                            midiMessage.getFloatVelocity());
        }
        else if (midiMessage.isNoteOff())
        {
            sampler->noteOff(midiMessage.getChannel(),
                             midiMessage.getNoteNumber(),
                             midiMessage.getFloatVelocity(),
                             true);
        }
        else if (midiMessage.isAllNotesOff() || midiMessage.isAllSoundOff())
        {
            sampler->allNotesOff(midiMessage.getChannel(), true);
        }
    }
}

void SamplerProcessor::setAttack(float newAttackTimeMs)
{
    attackTimeMs = newAttackTimeMs;
    updateVoiceParameters();
}

void SamplerProcessor::setRelease(float newReleaseTimeMs)
{
    releaseTimeMs = newReleaseTimeMs;
    updateVoiceParameters();
}

void SamplerProcessor::setGain(float newGain)
{
    gain = newGain;
}

void SamplerProcessor::updateVoiceParameters()
{
    double sampleRate = sampler->getSampleRate();
    if (sampleRate > 0)
    {
        for (int i = 0; i < sampler->getNumVoices(); ++i)
        {
            if (auto *voice = dynamic_cast<ProxySamplerVoice *>(sampler->getVoice(i)))
            {
                voice->setAttackRate(sampleRate, attackTimeMs);
                voice->setReleaseRate(sampleRate, releaseTimeMs);
            }
        }
    }
}