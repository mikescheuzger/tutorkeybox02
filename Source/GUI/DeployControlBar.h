#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "../Audio/AudioEngine.h"
#include "../Core/PresetManager.h"
#include "../Network/DeployClient.h"
#include "AudioSettingsWindow.h"
#include "MidiConsoleWindow.h"

// =============================================================================
// DeployControlBar — Header Bar with Dynamic Connection Status & Deploy Controls
//
// Features:
//   • Dynamic Hardware Connection Status Badge (Green = Pi Connected, Cyan = Standalone)
//   • One-Click "Deploy to KeyBox" Button with Container Transfer Progress Bar
//   • "Audio & MIDI Setup" Button (Opens AudioSettingsWindow)
//   • "MIDI Console Log" Button (Opens detached MidiConsoleWindow)
//   • Save & Load Preset JSON Buttons
// =============================================================================
class DeployControlBar : public juce::Component,
                         private juce::Timer {
public:
    DeployControlBar(AudioEngine& engineToControl, PresetManager& presetTarget);
    ~DeployControlBar() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    void updateConnectionStatus();
    void startDeployment();

    void setPresetName(const juce::String& name);
    void setSampleLoadingStatus(const juce::String& text, bool isReady = false);
    bool getIsPiConnected() const { return isPiConnected; }

private:
    AudioEngine&   audioEngine;
    PresetManager& presetManager;
    DeployClient   deployClient;

    juce::Label      statusBadgeLabel;
    juce::Label      presetBadgeLabel{ "PresetBadge", "PRESET: [Default Studio Piano]" };
    juce::Label      sampleLoadingStatusLabel;

    juce::TextButton deployButton{ "Deploy to KeyBox" };
    juce::ProgressBar progressBar{ transferProgress };
    double           transferProgress{ 0.0 };
    bool             isDeploying{ false };
    bool             isPiConnected{ false };
    juce::String     deployStatusMessage;

    juce::TextButton audioSettingsButton{ "Audio & MIDI Setup" };
    juce::TextButton midiConsoleButton{ "MIDI Console Log" };
    juce::TextButton savePresetButton{ "Save Preset" };
    juce::TextButton loadPresetButton{ "Load Preset" };

    std::unique_ptr<juce::FileChooser> fileChooser;
    std::unique_ptr<MidiConsoleWindow> consoleWindow;
    std::unique_ptr<AudioSettingsWindow> audioSettingsWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DeployControlBar)
};
