#include "PluginEditor.h"

ProxyAudioProcessorEditor::ProxyAudioProcessorEditor(ProxyAudioProcessor &p)
    : AudioProcessorEditor(&p),
      audioProcessor(p),
      layoutView(p.getSamplerProcessor())
{
    addAndMakeVisible(layoutView);

    // Start the timer for UI updates
    startTimerHz(30);

    // Set initial size
    setSize(CANVAS_WIDTH, CANVAS_HEIGHT);

    // Set minimum size constraints
    setResizeLimits(600, 400, 1200, 900);

    // Allow resizing
    setResizable(true, true);
}

ProxyAudioProcessorEditor::~ProxyAudioProcessorEditor()
{
    stopTimer();
}

void ProxyAudioProcessorEditor::paint(juce::Graphics &g)
{
    // Painting is handled by the LayoutView
}

void ProxyAudioProcessorEditor::resized()
{
    // Make the LayoutView fill the entire window
    layoutView.setBounds(getLocalBounds());
}

void ProxyAudioProcessorEditor::timerCallback()
{
    // Update UI with current meter levels
    float leftLevel = audioProcessor.getLeftLevel();
    float rightLevel = audioProcessor.getRightLevel();

    // Scale meter values for display (convert RMS to dB then to percentage)
    float leftDb = juce::Decibels::gainToDecibels(leftLevel, -60.0f);
    float rightDb = juce::Decibels::gainToDecibels(rightLevel, -60.0f);

    // Map from -60dB..0dB to 0%..100%
    leftLevel = juce::jmap(leftDb, -60.0f, 0.0f, 0.0f, 100.0f);
    rightLevel = juce::jmap(rightDb, -60.0f, 0.0f, 0.0f, 100.0f);

    // Clip to valid range
    leftLevel = juce::jlimit(0.0f, 100.0f, leftLevel);
    rightLevel = juce::jlimit(0.0f, 100.0f, rightLevel);

    // Update the layout view
    layoutView.updateLevels(leftLevel, rightLevel);

    // Update all voice positions
    layoutView.updateAllPlaybackPositions();
}