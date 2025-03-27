#include "LayoutView.h"
#include "BinaryData.h"

LayoutView::LayoutMessageHandler::LayoutMessageHandler(LayoutView &owner)
    : ownerView(owner)
{
}

bool LayoutView::LayoutMessageHandler::pageAboutToLoad(const juce::String &url)
{
    // Handle proxy: protocol for control messages
    if (url.startsWith("proxy:"))
    {
        juce::String params = url.fromFirstOccurrenceOf("proxy:", false, true);

        // Handle sampler parameters
        if (params.startsWith("sampler:"))
        {
            params = params.fromFirstOccurrenceOf("sampler:", false, true);

            if (params.startsWith("attack="))
            {
                float value = params.fromFirstOccurrenceOf("attack=", false, true).getFloatValue();
                ownerView.samplerProcessor.setAttack(value);
                return false;
            }
            else if (params.startsWith("release="))
            {
                float value = params.fromFirstOccurrenceOf("release=", false, true).getFloatValue();
                ownerView.samplerProcessor.setRelease(value);
                return false;
            }
            else if (params.startsWith("gain="))
            {
                float value = params.fromFirstOccurrenceOf("gain=", false, true).getFloatValue();
                ownerView.samplerProcessor.setGain(value);
                return false;
            }
            else if (params.startsWith("sample="))
            {
                juce::String sampleName = params.fromFirstOccurrenceOf("sample=", false, true);
                ownerView.samplerProcessor.setSample(sampleName);
                return false;
            }
            else if (params.startsWith("browseSample"))
            {
                // Open a file browser to load a new sample
                juce::FileChooser chooser("Select a Sample File...",
                                          juce::File::getSpecialLocation(juce::File::userMusicDirectory),
                                          "*.wav;*.aif;*.aiff;*.mp3");

                // Use launchAsync with a lambda callback instead of browseForFileToOpen
                chooser.launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                    [this](const juce::FileChooser &fc)
                                    {
                                        auto result = fc.getResult();
                                        if (result.exists())
                                        {
                                            ownerView.samplerProcessor.loadSample(result);
                                        }
                                    });

                return false;
            }
        }

        // We handled this URL
        return false;
    }
    // Handle custom font loading
    else if (url.startsWith("BinaryData::"))
    {
        // This is our own resource URL format
        juce::String resourceName = url.substring(12);
        int size = 0;
        const char *data = nullptr;

        // Get the binary data
        if (resourceName == "proxy_font_ttf")
        {
            data = BinaryData::getNamedResource("proxy_font_ttf", size);

            if (data != nullptr && size > 0)
            {
                // Convert binary data to base64
                // Fix: Cast size to size_t to avoid warning
                juce::MemoryBlock mb(data, static_cast<size_t>(size));
                juce::String base64 = mb.toBase64Encoding();

                // Create a data URL for the font
                juce::String dataUrl = juce::String("data:font/ttf;base64,") + base64;

                // Navigate to this data URL
                this->goToURL(dataUrl);
                return false; // We've handled this URL
            }
        }
    }

    return true; // We didn't handle this URL
}

// Main LayoutView implementation
LayoutView::LayoutView(SamplerProcessor &proc)
    : samplerProcessor(proc),
      pageLoaded(false),
      lastLeftLevel(0.0f),
      lastRightLevel(0.0f),
      lastPlaybackPosition(0),
      lastAttackMs(proc.getAttack()),
      lastReleaseMs(proc.getRelease()),
      lastGain(proc.getGain()),
      lastSampleName(proc.getCurrentSampleName())
{
    auto browser = new LayoutMessageHandler(*this);
    webView.reset(browser);
    // Update: Use the newer API to avoid deprecation warning
    webView->setFocusContainerType(juce::Component::FocusContainerType::none);
    addAndMakeVisible(webView.get());

    juce::String htmlContent = juce::String(BinaryData::layout_html, BinaryData::layout_htmlSize);
    juce::String cssContent = juce::String(BinaryData::layout_css, BinaryData::layout_cssSize);

    htmlContent = htmlContent.replace(
        "<link rel=\"stylesheet\" href=\"./layout.css\" />",
        "<style>\n" + cssContent + "\n    </style>");

    // Load the combined HTML content
    webView->goToURL(juce::String("data:text/html;charset=utf-8,") + htmlContent);
}

LayoutView::~LayoutView()
{
    webView = nullptr;
}

void LayoutView::paint(juce::Graphics &g)
{
    // Suppress unused parameter warning
    juce::ignoreUnused(g);
    // Nothing to paint - WebView handles rendering
}

void LayoutView::resized()
{
    // Make the webView take up all available space
    webView->setBounds(getLocalBounds());
}

void LayoutView::updateLevels(float leftLevel, float rightLevel)
{
    if (!pageLoaded)
        return;

    // Store the last known levels
    lastLeftLevel = leftLevel;
    lastRightLevel = rightLevel;

    // If signal is very close to zero, explicitly set it to zero
    if (leftLevel < 0.01f)
        leftLevel = 0.0f;
    if (rightLevel < 0.01f)
        rightLevel = 0.0f;

    try
    {
        // Update meters in the WebView - fix string concatenation
        juce::String script = juce::String("if (window.updateMeters) { window.updateMeters(") +
                              juce::String(leftLevel, 1) + juce::String(", ") +
                              juce::String(rightLevel, 1) + juce::String("); }");

        webView->evaluateJavascript(script);
    }
    catch (const std::exception &e)
    {
        // Log any errors for debugging
        juce::Logger::writeToLog("JavaScript error in meters: " + juce::String(e.what()));
    }
}

void LayoutView::updatePlaybackPosition(int position)
{
    if (!pageLoaded)
        return;

    lastPlaybackPosition = position;

    try
    {
        // Update waveform display in the WebView - fix string concatenation
        juce::String script = juce::String("if (window.updatePlaybackPosition) { window.updatePlaybackPosition(") +
                              juce::String(position) + juce::String("); }");

        webView->evaluateJavascript(script);
    }
    catch (const std::exception &e)
    {
        // Log any errors for debugging
        juce::Logger::writeToLog("JavaScript error in waveform: " + juce::String(e.what()));
    }
}

void LayoutView::timerCallback()
{
    // Check if the page is loaded
    if (!pageLoaded)
    {
        static int pageLoadCounter = 0;
        pageLoadCounter++;

        if (pageLoadCounter >= 10) // Wait about 333ms with 30Hz timer
        {
            pageLoaded = true;

            // Initialize with current values - fix string concatenation issue
            juce::String script = juce::String("if (window.initializeSampler) { window.initializeSampler({") +
                                  juce::String("attack: ") + juce::String(lastAttackMs) + juce::String(", ") +
                                  juce::String("release: ") + juce::String(lastReleaseMs) + juce::String(", ") +
                                  juce::String("gain: ") + juce::String(lastGain) + juce::String(", ") +
                                  juce::String("sampleName: '") + lastSampleName + juce::String("'") +
                                  "}); }";

            webView->evaluateJavascript(script);

            // Also update available samples list
            juce::StringArray samples = samplerProcessor.getAvailableSamples();
            juce::String samplesJson = "[";

            for (int i = 0; i < samples.size(); ++i)
            {
                samplesJson += "\"" + samples[i] + "\"";
                if (i < samples.size() - 1)
                    samplesJson += ",";
            }

            samplesJson += "]";

            // Fix string concatenation
            juce::String samplesScript = juce::String("if (window.updateSamplesList) { window.updateSamplesList(") +
                                         samplesJson + juce::String("); }");

            webView->evaluateJavascript(samplesScript);
        }

        return;
    }

    // Check for parameter changes in sampler processor
    float attackMs = samplerProcessor.getAttack();
    float releaseMs = samplerProcessor.getRelease();
    float gain = samplerProcessor.getGain();
    juce::String sampleName = samplerProcessor.getCurrentSampleName();

    bool paramsChanged = std::abs(attackMs - lastAttackMs) > 0.01f ||
                         std::abs(releaseMs - lastReleaseMs) > 0.01f ||
                         std::abs(gain - lastGain) > 0.01f ||
                         sampleName != lastSampleName;

    if (paramsChanged)
    {
        // Update sampler UI with current values - fix string concatenation
        juce::String script = juce::String("if (window.updateSamplerControls) { window.updateSamplerControls({") +
                              juce::String("attack: ") + juce::String(attackMs) + juce::String(", ") +
                              juce::String("release: ") + juce::String(releaseMs) + juce::String(", ") +
                              juce::String("gain: ") + juce::String(gain) + juce::String(", ") +
                              juce::String("sampleName: '") + sampleName + juce::String("'") +
                              "}); }";

        webView->evaluateJavascript(script);

        lastAttackMs = attackMs;
        lastReleaseMs = releaseMs;
        lastGain = gain;
        lastSampleName = sampleName;
    }
}