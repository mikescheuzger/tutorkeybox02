#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_extra/juce_gui_extra.h>

// =============================================================================
// AudioSettingsWindow — Floating Configuration Dialog for Audio & MIDI Devices
// =============================================================================
class AudioSettingsWindow : public juce::DocumentWindow {
public:
    explicit AudioSettingsWindow(juce::AudioDeviceManager& deviceManager);
    ~AudioSettingsWindow() override = default;

    void closeButtonPressed() override;

private:
    std::unique_ptr<juce::AudioDeviceSelectorComponent> selectorComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioSettingsWindow)
};
