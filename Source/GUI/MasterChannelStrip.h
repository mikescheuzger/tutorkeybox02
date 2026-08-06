#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "../Audio/AudioEngine.h"
#include "../Core/PresetManager.h"

// =============================================================================
// MasterChannelStrip — Logic Pro Inspired Right Panel Master Audio Strip
//
// Features:
//   • Compressor Plugin Box (Threshold, Ratio, Attack, Release)
//   • Limiter Plugin Box (Threshold)
//   • Soft Saturation Clipper Plugin Box (Threshold, Drive)
//   • Master Output Fader Slider
//   • Stereo Peak VU Meter Display (30 Hz real-time level refresh)
// =============================================================================
class MasterChannelStrip : public juce::Component,
                           private juce::Timer {
public:
    MasterChannelStrip(AudioEngine& engineToControl, PresetManager& presetTarget);
    ~MasterChannelStrip() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

private:
    AudioEngine&   audioEngine;
    PresetManager& presetManager;

    juce::Label headerLabel{ "MasterTitle", "MASTER CHANNEL" };

    // 1. Compressor Plugin Block
    juce::Label        compHeaderLabel{ "CompHeader", "1. COMPRESSOR" };
    juce::ToggleButton compToggle{ "ENABLE" };
    juce::Slider       compThresholdSlider;
    juce::Label        compThresholdLabel{ "CompThresh", "Thresh" };
    juce::Slider       compRatioSlider;
    juce::Label        compRatioLabel{ "CompRatio", "Ratio" };

    // 2. Limiter Plugin Block
    juce::Label        limHeaderLabel{ "LimHeader", "2. LIMITER" };
    juce::ToggleButton limToggle{ "ENABLE" };
    juce::Slider       limThresholdSlider;
    juce::Label        limThresholdLabel{ "LimThresh", "Thresh" };

    // 3. Soft Saturation Clipper Plugin Block
    juce::Label        clipHeaderLabel{ "ClipHeader", "3. SOFT CLIPPER" };
    juce::ToggleButton clipToggle{ "ENABLE" };
    juce::Slider       clipDriveSlider;
    juce::Label        clipDriveLabel{ "ClipDrive", "Drive" };

    // 4. Master Fader & VU Meter
    juce::Slider masterFader;
    juce::Label  masterFaderLabel{ "MasterFaderLabel", "Master Vol" };

    float currentPeakL{ 0.0f };
    float currentPeakR{ 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MasterChannelStrip)
};
