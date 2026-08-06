#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "../Audio/AudioEngine.h"
#include "../Core/PresetManager.h"
#include "../Network/DeployClient.h"
#include "MidiConsoleWindow.h"

// =============================================================================
// DeployControlBar — Header Bar with Dynamic Connection Status & Deploy Controls
//
// Features:
//   • Dynamic Hardware Connection Status Badge (Green = Pi Connected, Cyan = Standalone)
//   • One-Click "Deploy to KeyBox" Button with Container Transfer Progress Bar
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

private:
    AudioEngine&   audioEngine;
    PresetManager& presetManager;
    DeployClient   deployClient;

    juce::Label      statusBadgeLabel;
    juce::TextButton deployButton{ "Deploy to KeyBox" };
    juce::ProgressBar progressBar{ transferProgress };
    double           transferProgress{ 0.0 };
    bool             isDeploying{ false };
    bool             isPiConnected{ false };
    juce::String     deployStatusMessage;

    juce::TextButton midiConsoleButton{ "MIDI Console Log" };
    juce::TextButton savePresetButton{ "Save Preset" };
    juce::TextButton loadPresetButton{ "Load Preset" };

    std::unique_ptr<juce::FileChooser> fileChooser;
    std::unique_ptr<MidiConsoleWindow> consoleWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DeployControlBar)
};
