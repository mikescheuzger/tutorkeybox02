#include "AudioSettingsWindow.h"

AudioSettingsWindow::AudioSettingsWindow(juce::AudioDeviceManager& deviceManager)
    : DocumentWindow("Audio & MIDI Setup",
                     juce::Colour(0xff18181b),
                     DocumentWindow::closeButton) {

    selectorComponent = std::make_unique<juce::AudioDeviceSelectorComponent>(
        deviceManager,
        0, 2,  // min/max audio inputs
        0, 2,  // min/max audio outputs
        true,  // showChannelsAsStereoPairs
        true,  // showMidiInputOptions (USB Keyboards & MIDI controllers)
        false, // showMidiOutputSelector
        false  // showChannelsAsMonoOrStereo
    );

    setContentOwned(selectorComponent.release(), true);
    setResizable(true, false);
    setResizeLimits(480, 400, 700, 600);
    centreWithSize(520, 460);
}

void AudioSettingsWindow::closeButtonPressed() {
    setVisible(false);
}
