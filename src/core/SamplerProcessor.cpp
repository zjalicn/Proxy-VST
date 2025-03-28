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
          envelopeLevel(0.0),
          currentMidiNote(-1),
          sampleRate(44100.0), // Default sample rate
          shouldKill(false)
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

            // Store the MIDI note number
            currentMidiNote = midiNoteNumber;

            // Reset sample position for the new note
            sourceSamplePosition = 0.0;

            // Apply velocity scaling
            lgain = velocity;
            rgain = velocity;

            // Reset envelope
            envelopeLevel = 0.0;

            attackPhase = true;
            releasePhase = false;
            shouldKill = false;
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
            shouldKill = true;
        }
    }

    void setAttackRate(double newSampleRate, double attackTimeMs)
    {
        sampleRate = newSampleRate;
        attackSamples = sampleRate * (attackTimeMs / 1000.0);
        attackRate = attackSamples > 0 ? 1.0 / attackSamples : 1.0;
    }

    void setReleaseRate(double newSampleRate, double releaseTimeMs)
    {
        sampleRate = newSampleRate;
        releaseSamples = sampleRate * (releaseTimeMs / 1000.0);
        releaseRate = releaseSamples > 0 ? 1.0 / releaseSamples : 1.0;
    }

    void pitchWheelMoved(int /*newValue*/) override {}
    void controllerMoved(int /*controllerNumber*/, int /*newValue*/) override {}

    void renderNextBlock(juce::AudioBuffer<float> &outputBuffer, int startSample, int numSamples) override
    {
        if (shouldKill)
        {
            clearCurrentNote();
            return;
        }

        if (auto *playingSound = dynamic_cast<ProxySamplerSound *>(getCurrentlyPlayingSound().get()))
        {
            const juce::AudioBuffer<float> &audioData = playingSound->getAudioData();
            const float *const inL = audioData.getReadPointer(0);
            const float *const inR = audioData.getNumChannels() > 1 ? audioData.getReadPointer(1) : nullptr;

            float *outL = outputBuffer.getWritePointer(0, startSample);
            float *outR = outputBuffer.getNumChannels() > 1 ? outputBuffer.getWritePointer(1, startSample) : nullptr;

            // Working copies of member variables for better performance
            double localSourceSamplePosition = this->sourceSamplePosition;
            double localEnvelopeLevel = this->envelopeLevel;
            bool localAttackPhase = this->attackPhase;
            bool localReleasePhase = this->releasePhase;

            for (int sampleIndex = 0; sampleIndex < numSamples; ++sampleIndex)
            {
                // Current position in the sample
                int pos = static_cast<int>(localSourceSamplePosition);
                float alpha = static_cast<float>(localSourceSamplePosition - pos);
                float invAlpha = 1.0f - alpha;

                // Safety check for position
                if (pos >= audioData.getNumSamples() - 1)
                {
                    pos = audioData.getNumSamples() - 2;
                    if (pos < 0)
                        pos = 0;
                }
                int nextPos = pos + 1;

                // Update envelope
                if (localAttackPhase)
                {
                    localEnvelopeLevel += attackRate;
                    if (localEnvelopeLevel >= 1.0)
                    {
                        localEnvelopeLevel = 1.0;
                        localAttackPhase = false;
                    }
                }
                else if (localReleasePhase)
                {
                    localEnvelopeLevel -= releaseRate;
                    if (localEnvelopeLevel <= 0.0)
                    {
                        clearCurrentNote();
                        break;
                    }
                }

                // Get current sample with interpolation
                float l = (inL[pos] * invAlpha + inL[nextPos] * alpha) * static_cast<float>(localEnvelopeLevel);
                float r = inR != nullptr ? (inR[pos] * invAlpha + inR[nextPos] * alpha) * static_cast<float>(localEnvelopeLevel) : l;

                // Apply output gain
                if (outR != nullptr)
                {
                    outL[sampleIndex] += l * lgain;
                    outR[sampleIndex] += r * rgain;
                }
                else
                {
                    outL[sampleIndex] += l * lgain;
                }

                // Update source position
                localSourceSamplePosition += pitchRatio;

                // Handle loop or end of sample
                if (localSourceSamplePosition >= audioData.getNumSamples())
                {
                    if (!localReleasePhase)
                    {
                        localReleasePhase = true;
                    }
                    localSourceSamplePosition = 0.0;
                }
            }

            // Save back the updated values
            this->sourceSamplePosition = localSourceSamplePosition;
            this->envelopeLevel = localEnvelopeLevel;
            this->attackPhase = localAttackPhase;
            this->releasePhase = localReleasePhase;
            this->currentSamplePosition = static_cast<int>(localSourceSamplePosition);
        }
    }

    bool isVoiceActive() const
    {
        return getCurrentlyPlayingSound() != nullptr && !releasePhase && !shouldKill;
    }

    // Current sample position as a percentage of the total length
    double getCurrentSamplePosition() const
    {
        if (auto *sound = dynamic_cast<ProxySamplerSound *>(getCurrentlyPlayingSound().get()))
        {
            // Return current sample position as a proportion of the total sample length
            return sourceSamplePosition;
        }
        return 0.0;
    }

    int getCurrentMidiNote() const
    {
        return currentMidiNote;
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

    // MIDI and state
    int currentMidiNote;
    double sampleRate;
    bool shouldKill;
};

SamplerProcessor::SamplerProcessor()
    : currentSamplePosition(0),
      attackTimeMs(5.0f),    // 5ms default attack
      releaseTimeMs(100.0f), // 100ms default release
      gain(1.0f),
      monophonic(false), // Default to polyphonic
      lastMonophonicNote(-1)
{
    sampler = std::make_unique<juce::Synthesiser>();

    // Add voices to the sampler
    for (int i = 0; i < MAX_VOICES; ++i)
    {
        sampler->addVoice(new ProxySamplerVoice());
    }

    // Initialize voice positions
    for (auto &voicePos : voicePositions)
    {
        voicePos.position = 0;
        voicePos.isActive = false;
    }

    // Initialize anti-pop buffer
    antiPopBuffer.setSize(2, 512); // 2 channels, small buffer
    antiPopBuffer.clear();

    // Load the default samples
    loadDefaultSamples();
}

SamplerProcessor::~SamplerProcessor()
{
}

void SamplerProcessor::loadDefaultSamples()
{
    // Scan the user's samples folder in Documents/Proxy/Samples
    juce::StringArray userSamples = sampleLibrary.scanUserSamplesFolder();

    // If user samples were found, use the first one as default
    if (userSamples.size() > 0)
    {
        setSample(userSamples[0]);
    }
    // No fallback to built-in samples - we'll just display a message when no samples are found
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

    // Check for monophonic mode and handle it specially
    if (monophonic && !midiMessages.isEmpty())
    {
        // Capture current audio state before processing new notes (anti-pop mechanism)
        captureAntiPopBuffer(buffer);

        // Process all MIDI events in this block
        juce::MidiBuffer::Iterator it(midiMessages);
        juce::MidiMessage message;
        int samplePosition;

        // Find the latest note-on message in this block
        int latestNoteOn = -1;
        int latestNoteOnPos = -1;

        while (it.getNextEvent(message, samplePosition))
        {
            if (message.isNoteOn())
            {
                latestNoteOn = message.getNoteNumber();
                latestNoteOnPos = samplePosition;
                lastMonophonicNote = latestNoteOn;
            }
            else if (message.isNoteOff() && message.getNoteNumber() == lastMonophonicNote)
            {
                // Only process note-offs for the current note
                handleMidiEvent(message);
            }
        }

        // Clear existing MIDI messages and only process the latest note-on if there is one
        midiMessages.clear();

        if (latestNoteOn >= 0)
        {
            // Stop all playing notes first
            sampler->allNotesOff(1, false);

            // Create a new note-on message for the latest note
            juce::MidiMessage newNoteOn = juce::MidiMessage::noteOn(1, latestNoteOn, (uint8_t)100);
            midiMessages.addEvent(newNoteOn, latestNoteOnPos);
        }
    }

    // Process incoming MIDI messages
    for (const auto &metadata : midiMessages)
    {
        handleMidiEvent(metadata.getMessage());
    }

    // Render the sampler audio
    sampler->renderNextBlock(buffer, midiMessages, 0, buffer.getNumSamples());

    // Apply monophonic mode anti-pop processing if we have captured a buffer
    if (monophonic && antiPopCaptured)
    {
        applyAntiPopProcessing(buffer);
    }

    // Apply gain
    if (gain != 1.0f)
    {
        buffer.applyGain(gain);
    }

    // Update voice positions for display purposes
    updateVoicePositions();
}

void SamplerProcessor::captureAntiPopBuffer(const juce::AudioBuffer<float> &buffer)
{
    // Only capture if any voice is active
    if (!isAnyVoiceActive())
    {
        antiPopCaptured = false;
        return;
    }

    // Make sure our anti-pop buffer is the right size
    const int samplesToCapture = juce::jmin(buffer.getNumSamples(), antiPopBuffer.getNumSamples());

    // Resize if needed (keeping data)
    if (antiPopBuffer.getNumSamples() < samplesToCapture)
    {
        antiPopBuffer.setSize(buffer.getNumChannels(), samplesToCapture, true, true, true);
    }

    // Capture a silent buffer to be filled by the sampler
    antiPopBuffer.clear();

    // Flag that we've captured
    antiPopCaptured = true;

    // Remember the current voice states to restore later if needed
    // This is a simplification - in a real implementation you'd store more state
    sampler->renderNextBlock(antiPopBuffer, juce::MidiBuffer(), 0, samplesToCapture);
}

void SamplerProcessor::applyAntiPopProcessing(juce::AudioBuffer<float> &buffer)
{
    if (!antiPopCaptured)
        return;

    const int numSamples = juce::jmin(buffer.getNumSamples(), antiPopBuffer.getNumSamples());
    const int numChannels = juce::jmin(buffer.getNumChannels(), antiPopBuffer.getNumChannels());

    // Apply a short crossfade between the old and new audio
    for (int channel = 0; channel < numChannels; ++channel)
    {
        float *bufferData = buffer.getWritePointer(channel);
        const float *antiPopData = antiPopBuffer.getReadPointer(channel);

        for (int i = 0; i < numSamples; ++i)
        {
            // Short crossfade - 32 samples is about 0.7ms at 44.1kHz
            const int fadeSamples = 32;

            if (i < fadeSamples)
            {
                float alpha = static_cast<float>(i) / fadeSamples;
                bufferData[i] = bufferData[i] * alpha + antiPopData[i] * (1.0f - alpha);
            }
        }
    }

    // Reset for next time
    antiPopCaptured = false;
}

void SamplerProcessor::updateVoicePositions()
{
    // Reset all voice positions
    for (auto &voicePos : voicePositions)
    {
        voicePos.isActive = false;
    }

    // Update positions for active voices
    int activeVoiceCount = 0;

    for (int i = 0; i < sampler->getNumVoices() && activeVoiceCount < MAX_VOICES; ++i)
    {
        if (auto *voice = dynamic_cast<ProxySamplerVoice *>(sampler->getVoice(i)))
        {
            if (voice->isVoiceActive())
            {
                voicePositions[activeVoiceCount].position = static_cast<int>(voice->getCurrentSamplePosition());
                voicePositions[activeVoiceCount].isActive = true;
                activeVoiceCount++;
            }
        }
    }

    // Store the position of the first voice (for backward compatibility)
    if (activeVoiceCount > 0)
    {
        currentSamplePosition = voicePositions[0].position;
    }
}

bool SamplerProcessor::isAnyVoiceActive() const
{
    if (sampler == nullptr)
        return false;

    for (int i = 0; i < sampler->getNumVoices(); ++i)
    {
        if (auto *voice = dynamic_cast<ProxySamplerVoice *>(sampler->getVoice(i)))
        {
            if (voice->isVoiceActive())
                return true;
        }
    }

    return false;
}

void SamplerProcessor::reset()
{
    sampler->allNotesOff(0, false);
    antiPopCaptured = false;
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
        // Everything is handled in the processBlock method for monophonic mode
        if (!monophonic)
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
        else
        {
            // For monophonic mode, only handle note-offs and other control messages here
            // Note-ons are handled in processBlock to ensure only one note plays
            if (midiMessage.isNoteOff())
            {
                sampler->noteOff(midiMessage.getChannel(),
                                 midiMessage.getNoteNumber(),
                                 midiMessage.getFloatVelocity(),
                                 true);
            }
            else if (midiMessage.isAllNotesOff() || midiMessage.isAllSoundOff())
            {
                sampler->allNotesOff(midiMessage.getChannel(), true);
                lastMonophonicNote = -1;
            }
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

void SamplerProcessor::setMonophonic(bool isMonophonic)
{
    // Only process if the state actually changed
    if (monophonic != isMonophonic)
    {
        monophonic = isMonophonic;
        lastMonophonicNote = -1;

        // Stop all notes when switching modes
        sampler->allNotesOff(1, false);
    }
}

void SamplerProcessor::updateActiveVoices()
{
    // Only needed for the old implementation
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

void SamplerProcessor::refreshSamples()
{
    // Get the current sample name (to restore selection if possible)
    juce::String currentSample = getCurrentSampleName();

    // Scan for samples
    juce::StringArray samples = sampleLibrary.scanUserSamplesFolder();

    // If no samples found, just return
    if (samples.isEmpty())
    {
        return;
    }

    // Try to restore the previous sample selection
    if (samples.contains(currentSample))
    {
        setSample(currentSample);
    }
    else
    {
        // If previous sample not found, use the first one
        setSample(samples[0]);
    }
}