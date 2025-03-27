![Preview Image](preview.png)

A sample-based rompler instrument plugin with post processing effects.

## Features

- Sample-based playback with pitch shifting based on MIDI notes
- Sample browser with ability to load custom samples
- Adjustable attack and release parameters
- Real-time waveform visualization with playback position

## Build & Installation

1. After cloning the repository, initialize and update the JUCE submodule:

   ```
   git submodule add https://github.com/juce-framework/JUCE.git
   git submodule update --init --recursive
   ```

2. Build the project:

   ```
   cmake -B build
   cmake --build build
   ```

3. The plugin should automatically be copied to your system's VST folder:
   - Windows: `C:\Program Files\Common Files\VST3`
   - macOS: `~/Library/Audio/Plug-Ins/VST3`
   - Linux: `~/.vst3`
