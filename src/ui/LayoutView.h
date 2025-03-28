#pragma once

#include <JuceHeader.h>
#include "SamplerProcessor.h"

class LayoutView : public juce::Component, private juce::Timer
{
public:
    LayoutView(SamplerProcessor &samplerProcessor);
    ~LayoutView() override;

    void paint(juce::Graphics &g) override;
    void resized() override;

    // Update UI with current levels
    void updateLevels(float leftLevel, float rightLevel);

    // Update UI with current playback position
    void updatePlaybackPosition(int position);

    // Update UI with all active voice positions
    void updateAllPlaybackPositions();

    // Update waveform display with actual sample data
    void updateWaveformDisplay();

    // Update samples list
    void updateSamplesList();

    // Custom web view that handles our custom URL scheme
    class LayoutMessageHandler : public juce::WebBrowserComponent
    {
    public:
        LayoutMessageHandler(LayoutView &owner);
        bool pageAboutToLoad(const juce::String &url) override;

    private:
        LayoutView &ownerView;
    };

private:
    SamplerProcessor &samplerProcessor;

    std::unique_ptr<juce::WebBrowserComponent> webView;

    // UI state
    bool pageLoaded;
    float lastLeftLevel, lastRightLevel;
    int lastPlaybackPosition;
    bool voicesActive;

    // Store the last voice positions to check for changes
    std::array<VoicePosition, SamplerProcessor::MAX_VOICES> lastVoicePositions;

    // Sampler parameters
    float lastAttackMs;
    float lastReleaseMs;
    float lastGain;
    bool lastMonophonic;
    juce::String lastSampleName;

    // Timer for UI updates
    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LayoutView)
};