#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "LayoutView.h"

class ProxyAudioProcessorEditor : public juce::AudioProcessorEditor,
                                  private juce::Timer
{
public:
    static constexpr int CANVAS_WIDTH = 800;
    static constexpr int CANVAS_HEIGHT = 600;

    ProxyAudioProcessorEditor(ProxyAudioProcessor &);
    ~ProxyAudioProcessorEditor() override;

    void paint(juce::Graphics &) override;
    void resized() override;

private:
    ProxyAudioProcessor &audioProcessor;

    LayoutView layoutView;

    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProxyAudioProcessorEditor)
};