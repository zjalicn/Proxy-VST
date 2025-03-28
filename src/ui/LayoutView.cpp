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
                sampleName = juce::URL::removeEscapeChars(sampleName);

                bool success = ownerView.samplerProcessor.setSample(sampleName);

                if (success)
                {
                    ownerView.updateWaveformDisplay(); // Update the waveform display directly
                }
                return false;
            }
            else if (params.startsWith("refreshSamples"))
            {
                ownerView.samplerProcessor.refreshSamples();

                // Update the UI with the new samples list
                ownerView.updateSamplesList();

                // Also update the waveform for the current sample
                ownerView.updateWaveformDisplay();

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

                                            // Update waveform display after loading new sample
                                            ownerView.updateWaveformDisplay();
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
      voicesActive(false),
      lastAttackMs(proc.getAttack()),
      lastReleaseMs(proc.getRelease()),
      lastGain(proc.getGain()),
      lastSampleName(proc.getCurrentSampleName())
{
    auto browser = new LayoutMessageHandler(*this);
    webView.reset(browser);
    webView->setFocusContainerType(juce::Component::FocusContainerType::none);
    addAndMakeVisible(webView.get());

    juce::String htmlContent = juce::String(BinaryData::layout_html, BinaryData::layout_htmlSize);
    juce::String cssContent = juce::String(BinaryData::layout_css, BinaryData::layout_cssSize);

    // Update HTML content to include div containers for multiple playheads
    htmlContent = htmlContent.replace(
        "<div id=\"playbackPosition\" class=\"editor__waveform-progress\"></div>",
        "<div id=\"playheadsContainer\">\n"
        "  <div id=\"playbackPosition\" class=\"editor__waveform-progress\"></div>\n"
        "  <div id=\"playbackPosition1\" class=\"editor__waveform-progress\"></div>\n"
        "  <div id=\"playbackPosition2\" class=\"editor__waveform-progress\"></div>\n"
        "  <div id=\"playbackPosition3\" class=\"editor__waveform-progress\"></div>\n"
        "  <div id=\"playbackPosition4\" class=\"editor__waveform-progress\"></div>\n"
        "  <div id=\"playbackPosition5\" class=\"editor__waveform-progress\"></div>\n"
        "  <div id=\"playbackPosition6\" class=\"editor__waveform-progress\"></div>\n"
        "  <div id=\"playbackPosition7\" class=\"editor__waveform-progress\"></div>\n"
        "</div>");

    // Add CSS for playheads
    cssContent += "\n\n/* Multiple Playheads Styling */\n"
                  "#playheadsContainer { position: absolute; top: 0; left: 0; width: 100%; height: 100%; }\n"
                  ".editor__waveform-progress { display: none; }\n";

    htmlContent = htmlContent.replace(
        "<link rel=\"stylesheet\" href=\"./layout.css\" />",
        "<style>\n" + cssContent + "\n    </style>");

    // Add JavaScript for handling multiple playheads
    juce::String multiplePlayheadsScript = R"(
    // Handle multiple playheads
    window.updateMultiplePlayheads = function(positions, totalSampleLength) {
        // Base color for the first playhead (primary teal color)
        const baseColor = [0, 188, 212]; // RGB for #00bcd4
        
        for (let i = 0; i < positions.length; i++) {
            const position = positions[i];
            const isActive = position.isActive;
            const playheadElement = document.getElementById(i === 0 ? 'playbackPosition' : ('playbackPosition' + i));
            
            if (!playheadElement) continue;
            
            if (isActive) {
                // Calculate position in pixels
                const positionRatio = position.position / totalSampleLength;
                const canvasWidth = document.getElementById('waveformDisplay').width;
                const positionX = Math.min(positionRatio * canvasWidth, canvasWidth);
                
                // Set position
                playheadElement.style.left = positionX + 'px';
                
                // Adjust color (each subsequent playhead gets lighter)
                const opacity = 1.0 - (i * 0.1); // Reduce opacity slightly for each voice
                const lightnessIncrease = i * 12; // Make each voice progressively lighter
                
                // Create RGB color with increasing lightness
                const r = Math.min(255, baseColor[0] + lightnessIncrease);
                const g = Math.min(255, baseColor[1] + lightnessIncrease);
                const b = Math.min(255, baseColor[2]);
                
                playheadElement.style.backgroundColor = `rgba(${r}, ${g}, ${b}, ${opacity})`;
                playheadElement.style.display = 'block';
            } else {
                playheadElement.style.display = 'none';
            }
        }
    };
    )";

    // Insert the script just before the closing </script> tag
    size_t scriptEndPos = htmlContent.lastIndexOf("</script>");
    if (scriptEndPos != -1)
    {
        htmlContent = htmlContent.replaceSection(scriptEndPos, 0, multiplePlayheadsScript);
    }

    // Load the combined HTML content
    webView->goToURL(juce::String("data:text/html;charset=utf-8,") + htmlContent);

    // Initialize voice positions array
    for (auto &voicePos : lastVoicePositions)
    {
        voicePos.position = 0;
        voicePos.isActive = false;
    }

    // Start the timer for UI updates
    startTimerHz(30);
}

LayoutView::~LayoutView()
{
    stopTimer();
    webView = nullptr;
}

void LayoutView::paint(juce::Graphics &g)
{
    juce::ignoreUnused(g);
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
        // Update waveform display in the WebView
        juce::String script = juce::String("if (window.updatePlaybackPosition) { window.updatePlaybackPosition(") +
                              juce::String(position) + juce::String("); }");

        // Add visibility check to the script
        script += juce::String("\nif (document.getElementById('playbackPosition')) { document.getElementById('playbackPosition').style.display = '") +
                  juce::String(samplerProcessor.isAnyVoiceActive() ? "block" : "none") + juce::String("'; }");

        webView->evaluateJavascript(script);
    }
    catch (const std::exception &e)
    {
        // Log any errors for debugging
        juce::Logger::writeToLog("JavaScript error in waveform: " + juce::String(e.what()));
    }
}

void LayoutView::updateAllPlaybackPositions()
{
    if (!pageLoaded)
        return;

    // Get all voice positions from the sampler
    const auto &voicePositions = samplerProcessor.getAllVoicePositions();

    // Convert voice positions to JSON for JavaScript
    juce::String positionsJson = "[";

    for (int i = 0; i < SamplerProcessor::MAX_VOICES; ++i)
    {
        positionsJson += "{\"position\":" + juce::String(voicePositions[i].position) +
                         ",\"isActive\":" + (voicePositions[i].isActive ? "true" : "false") + "}";

        if (i < SamplerProcessor::MAX_VOICES - 1)
            positionsJson += ",";
    }

    positionsJson += "]";

    // Get sample data for the total length
    auto sampleName = samplerProcessor.getCurrentSampleName();
    auto sampleData = samplerProcessor.getSampleLibrary().getSampleAudioBuffer(sampleName);
    int totalSampleLength = sampleData.buffer ? sampleData.buffer->getNumSamples() : 0;

    // Call the JavaScript function to update all playheads
    juce::String script = "if (window.updateMultiplePlayheads) { window.updateMultiplePlayheads(" +
                          positionsJson + ", " + juce::String(totalSampleLength) + "); }";

    try
    {
        webView->evaluateJavascript(script);
    }
    catch (const std::exception &e)
    {
        // Log any errors for debugging
        juce::Logger::writeToLog("JavaScript error in multiple playheads: " + juce::String(e.what()));
    }
}

void LayoutView::updateSamplesList()
{
    if (!pageLoaded)
        return;

    // Get the list of available samples
    juce::StringArray samples = samplerProcessor.getAvailableSamples();

    // Create the JSON string array
    juce::String samplesJson = "[";

    for (int i = 0; i < samples.size(); ++i)
    {
        // Ensure proper JSON encoding of the sample name with escaped quotes
        juce::String jsonEscapedName = samples[i].replace("\\", "\\\\").replace("\"", "\\\"");
        samplesJson += "\"" + jsonEscapedName + "\"";
        if (i < samples.size() - 1)
            samplesJson += ",";
    }

    samplesJson += "]";

    // Update the samples list in the web view
    juce::String samplesScript = juce::String("if (window.updateSamplesList) { window.updateSamplesList(") +
                                 samplesJson + juce::String("); }");

    webView->evaluateJavascript(samplesScript);
}

void LayoutView::updateWaveformDisplay()
{
    if (!pageLoaded)
        return;

    // Get current sample data
    auto sampleName = samplerProcessor.getCurrentSampleName();
    auto sampleData = samplerProcessor.getSampleLibrary().getSampleAudioBuffer(sampleName);

    if (sampleData.buffer && sampleData.buffer->getNumSamples() > 0)
    {
        // We'll send a downsampled version of the waveform data to JavaScript
        const int channelsToUse = juce::jmin(sampleData.buffer->getNumChannels(), 2);
        const int numSamples = sampleData.buffer->getNumSamples();

        // Max number of points to send (to keep JavaScript performant)
        const int maxPoints = 1000;
        const int skipFactor = juce::jmax(1, numSamples / maxPoints);

        juce::String waveformData = "[";

        for (int channel = 0; channel < channelsToUse; ++channel)
        {
            waveformData += "[";
            const float *channelData = sampleData.buffer->getReadPointer(channel);

            for (int i = 0; i < numSamples; i += skipFactor)
            {
                waveformData += juce::String(channelData[i]);

                if (i + skipFactor < numSamples)
                    waveformData += ",";
            }

            waveformData += "]";

            if (channel < channelsToUse - 1)
                waveformData += ",";
        }

        waveformData += "]";

        // Send waveform data to JavaScript
        juce::String script = "if (window.setWaveformData) { window.setWaveformData(" +
                              waveformData + ", " +
                              juce::String(numSamples) + "); }";

        webView->evaluateJavascript(script);
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
                                  juce::String("sampleName: '") + lastSampleName.replace("'", "\\'") + juce::String("'") +
                                  "}); }";

            webView->evaluateJavascript(script);

            // Update the samples list
            updateSamplesList();

            // Initialize waveform display with current sample data
            updateWaveformDisplay();
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
        // Escape any quotes in the sample name for JavaScript
        juce::String escapedSampleName = sampleName.replace("'", "\\'");

        // Update sampler UI with current values - fix string concatenation
        juce::String script = juce::String("if (window.updateSamplerControls) { window.updateSamplerControls({") +
                              juce::String("attack: ") + juce::String(attackMs) + juce::String(", ") +
                              juce::String("release: ") + juce::String(releaseMs) + juce::String(", ") +
                              juce::String("gain: ") + juce::String(gain) + juce::String(", ") +
                              juce::String("sampleName: '") + escapedSampleName + juce::String("'") +
                              "}); }";

        webView->evaluateJavascript(script);

        // If the sample has changed, update the waveform display
        if (sampleName != lastSampleName)
        {
            updateWaveformDisplay();
        }

        lastAttackMs = attackMs;
        lastReleaseMs = releaseMs;
        lastGain = gain;
        lastSampleName = sampleName;
    }

    // Check for voice position changes
    const auto &currentVoicePositions = samplerProcessor.getAllVoicePositions();
    bool positionsChanged = false;

    for (int i = 0; i < SamplerProcessor::MAX_VOICES; ++i)
    {
        if (currentVoicePositions[i].isActive != lastVoicePositions[i].isActive ||
            (currentVoicePositions[i].isActive &&
             currentVoicePositions[i].position != lastVoicePositions[i].position))
        {
            positionsChanged = true;
            break;
        }
    }

    // Update voice activity state
    bool currentlyActive = samplerProcessor.isAnyVoiceActive();
    if (voicesActive != currentlyActive || positionsChanged)
    {
        voicesActive = currentlyActive;

        // Update all playback positions
        updateAllPlaybackPositions();

        // Update last positions
        for (int i = 0; i < SamplerProcessor::MAX_VOICES; ++i)
        {
            lastVoicePositions[i] = currentVoicePositions[i];
        }
    }
}