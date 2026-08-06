#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "../Audio/AudioEngine.h"
#include "../Core/PresetManager.h"
#include "../Synth/SampleContainerReader.h"
#include "InstrumentEditorWindow.h"

// =============================================================================
// LayerCardComponent — Modular Synth Layer Card (Layers 1 to 4)
//
// Features:
//   • Instrument Title & Active Instrument Badge (Click opens InstrumentEditorWindow)
//   • Clear Layer Button
//   • Resonant Low-Pass Filter Knobs (Cutoff & Resonance)
//   • AUX Send Knob (Routes signal to 5th FX Card)
//   • Channel Card Output Volume Fader & Mute Toggle
//   • Drag & Drop Dropzone for .bin sample container files
// =============================================================================
class LayerCardComponent : public juce::Component,
                           public juce::FileDragAndDropTarget {
public:
    LayerCardComponent(AudioEngine& engineToControl, PresetManager& presetMgr, int layerIndex);
    ~LayerCardComponent() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void updateFromPreset();

    // ── juce::FileDragAndDropTarget ───────────────────────────────────────────
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

private:
    AudioEngine&   audioEngine;
    PresetManager& presetManager;
    int            layerIndex{ 0 };
    bool           isDragHovering{ false };

    juce::Label      titleLabel;
    juce::TextButton instrumentBadgeButton;
    juce::TextButton clearButton{ "Clear" };

    // Filter & AUX Send Controls
    juce::Label  filterHeaderLabel{ "FilterHeader", "RESONANT LPF & AUX SEND" };
    juce::Slider cutoffKnob;
    juce::Label  cutoffLabel{ "Cutoff", "Cutoff" };
    juce::Slider resonanceKnob;
    juce::Label  resonanceLabel{ "Res", "Res" };
    juce::Slider auxSendKnob;
    juce::Label  auxSendLabel{ "AUX Send", "AUX Send" };

    // Volume & Mute Controls
    juce::Slider      volumeSlider;
    juce::Label       volumeLabel{ "Vol", "Vol" };
    juce::ToggleButton muteToggle{ "MUTE" };

    std::unique_ptr<InstrumentEditorWindow> editorWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LayerCardComponent)
};
