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
          fadingSamplePosition(-1)
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

            // If we are already playing and being reused (monophonic mode), start a crossfade
            if (getCurrentlyPlayingSound() != nullptr && !releasePhase)
            {
                // Save current position for crossfade
                fadingSamplePosition = sourceSamplePosition;
                fadingEnvelopeLevel = envelopeLevel;
                fadeOutCounter = 0;
                isFading = true;
            }
            else
            {
                isFading = false;
                fadingSamplePosition = -1;
            }

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

            // For crossfading
            double localFadingSamplePosition = this->fadingSamplePosition;
            const double FADE_TIME_SAMPLES = 300.0; // Crossfade over a short time to avoid clicks

            while (--numSamples >= 0)
            {
                // Process the new note
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
                float r = inR != nullptr ? (inR[pos] * invAlpha + inR[nextPos] * alpha) * (float)envelopeLevel : l;

                // Apply crossfade if needed (for monophonic mode)
                if (isFading && localFadingSamplePosition >= 0)
                {
                    // Calculate position for fading note
                    auto fadingPos = (int)localFadingSamplePosition;
                    auto fadingAlpha = (float)(localFadingSamplePosition - fadingPos);
                    auto fadingInvAlpha = 1.0f - fadingAlpha;

                    // Simple linear interpolation for fading note
                    int fadingNextPos = fadingPos + 1;
                    if (fadingNextPos >= audioData.getNumSamples())
                        fadingNextPos = fadingPos;

                    // Calculate fade out factor (linear fade)
                    float fadeOutFactor = juce::jmax(0.0f, (float)(1.0 - (fadeOutCounter / FADE_TIME_SAMPLES)));
                    fadeOutCounter++;

                    if (fadeOutFactor > 0.0f)
                    {
                        // Apply the fading envelope
                        float fadingEnvelope = fadingEnvelopeLevel * fadeOutFactor;

                        // Get samples for the fading note
                        float fadingL = (inL[fadingPos] * fadingInvAlpha + inL[fadingNextPos] * fadingAlpha) * fadingEnvelope;
                        float fadingR = inR != nullptr ? (inR[fadingPos] * fadingInvAlpha + inR[fadingNextPos] * fadingAlpha) * fadingEnvelope
                                                       : fadingL;

                        // Mix the fading note with the current note
                        l += fadingL;
                        r += fadingR;

                        // Update the fading position
                        localFadingSamplePosition += pitchRatio;
                        if (localFadingSamplePosition >= audioData.getNumSamples() || fadeOutCounter >= FADE_TIME_SAMPLES)
                        {
                            isFading = false;
                            localFadingSamplePosition = -1;
                        }
                    }
                    else
                    {
                        isFading = false;
                        localFadingSamplePosition = -1;
                    }
                }

                // Apply the final gain
                if (outR != nullptr)
                {
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
            this->fadingSamplePosition = localFadingSamplePosition;
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

    // For smooth crossfading in monophonic mode
    double fadingSamplePosition;
    double fadingEnvelopeLevel;
    double fadeOutCounter;
    bool isFading;
};

SamplerProcessor::SamplerProcessor()
    : currentSamplePosition(0),
      attackTimeMs(5.0f),    // 5ms default attack
      releaseTimeMs(100.0f), // 100ms default release
      gain(1.0f),
      monophonic(false) // Default to polyphonic
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

    // Update voice positions for display purposes
    updateVoicePositions();
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
            // If monophonic mode is enabled, stop all playing notes with a proper tail-off
            // so we can smoothly crossfade to the new note
            if (monophonic)
            {
                for (int i = 0; i < sampler->getNumVoices(); ++i)
                {
                    auto *voice = sampler->getVoice(i);
                    if (voice->isPlayingChannel(midiMessage.getChannel()) &&
                        voice->isVoiceActive())
                    {
                        // Use true for allowTailOff to get a smooth transition
                        voice->stopNote(1.0f, true);
                    }
                }
            }

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

void SamplerProcessor::setMonophonic(bool isMonophonic)
{
    // Only process if the state actually changed
    if (monophonic != isMonophonic)
    {
        monophonic = isMonophonic;

        // When switching to monophonic mode, we don't need to forcibly stop all notes
        // The proper note handling will happen in handleMidiEvent when new notes are played
    }
}

void SamplerProcessor::updateActiveVoices()
{
    // If in monophonic mode, make sure only one voice can be active
    if (monophonic)
    {
        // Find the most recently activated voice
        juce::SynthesiserVoice *activeVoice = nullptr;

        for (int i = 0; i < sampler->getNumVoices(); ++i)
        {
            if (auto *voice = sampler->getVoice(i))
            {
                if (voice->isVoiceActive() && voice->getCurrentlyPlayingNote() > 0)
                {
                    if (activeVoice == nullptr)
                    {
                        activeVoice = voice;
                    }
                }
            }
        }

        // Make sure only that voice is sounding
        if (activeVoice != nullptr)
        {
            for (int i = 0; i < sampler->getNumVoices(); ++i)
            {
                if (auto *voice = sampler->getVoice(i))
                {
                    if (voice != activeVoice && voice->isVoiceActive())
                    {
                        voice->stopNote(0.0f, true); // Use smooth release
                    }
                }
            }
        }
    }
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