#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "../Audio/AudioEngine.h"

// =============================================================================
// FXCardComponent — 5th Master FX Bus Control Card Component
//
// Layout Order (As requested):
//   1. Stereo Delay Subsystem (Delay Time ms, Feedback, Wet Level)
//   2. Stereo Reverb Subsystem (Room Size, Damping, Wet Level, Dry Level)
//   3. FX Bus Output Gain
// =============================================================================
class FXCardComponent : public juce::Component {
public:
    explicit FXCardComponent(AudioEngine& engineToControl);
    ~FXCardComponent() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void updateFromPreset();

private:
    AudioEngine& audioEngine;

    juce::Label headerLabel{ "FXCardTitle", "MASTER FX BUS" };

    // Section 1: Stereo Delay Controls (FIRST)
    juce::Label       delayHeaderLabel{ "DelayTitle", "1. STEREO DELAY" };
    juce::ToggleButton delayToggle{ "Enable Delay" };
    juce::Slider      delayTimeSlider;
    juce::Label       delayTimeLabel{ "Time", "Time ms" };
    juce::Slider      delayFeedbackSlider;
    juce::Label       delayFeedbackLabel{ "Fdbk", "Feedback" };
    juce::Slider      delayWetSlider;
    juce::Label       delayWetLabel{ "DelayWet", "Wet Level" };

    // Section 2: Stereo Reverb Controls (SECOND)
    juce::Label       reverbHeaderLabel{ "ReverbTitle", "2. STEREO REVERB" };
    juce::ToggleButton reverbToggle{ "Enable Reverb" };
    juce::Slider      reverbRoomSlider;
    juce::Label       reverbRoomLabel{ "Size", "Room Size" };
    juce::Slider      reverbDampingSlider;
    juce::Label       reverbDampingLabel{ "Damp", "Damping" };
    juce::Slider      reverbWetSlider;
    juce::Label       reverbWetLabel{ "RevWet", "Wet Level" };

    // Master FX Output Gain
    juce::Slider fxOutputSlider;
    juce::Label  fxOutputLabel{ "FXGain", "Master FX Vol" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FXCardComponent)
};
