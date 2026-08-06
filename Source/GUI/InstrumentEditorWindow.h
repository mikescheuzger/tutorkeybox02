#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "../Audio/AudioEngine.h"
#include "../Core/PresetManager.h"
#include "../Synth/SampleContainerReader.h"

// =============================================================================
// InstrumentEditorWindow — Deep Instrument Sound Design & ADSR Editor Window
//
// Features:
//   • Instrument Selection Dropdown (ComboBox of prepared .bin container files)
//   • Hardware Sync Badge Indicator ("Mac Engine Only" vs "KeyBox Hardware Synced")
//   • Raw Sample Input Gain Knob / Slider
//   • Interactive Draggable ADSR Curve Canvas (Attack, Decay, Sustain, Release)
//   • Animated Waveform Viewer with real-time Playhead position marker
//   • Velocity Zone & Round-Robin Sample Map Overview
// =============================================================================
class InstrumentEditorWindow : public juce::DocumentWindow,
                               public juce::ChangeListener,
                               private juce::Timer {
public:
    InstrumentEditorWindow(AudioEngine& audioEngine, PresetManager& presetManager, int layerIndex);
    ~InstrumentEditorWindow() override;

    void closeButtonPressed() override;
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    void timerCallback() override;

private:
    AudioEngine&   audioEngine;
    PresetManager& presetManager;
    int            layerIndex{ 0 };

    // Interactive ADSR Curve Canvas
    class AdsrCanvas : public juce::Component {
    public:
        AdsrCanvas(PresetManager& presetMgr, LayeredSynth& synthRef, int layerIdx);
        void updateFromPreset();
        void paint(juce::Graphics& g) override;
        void resized() override;
        void mouseDown(const juce::MouseEvent& e) override;
        void mouseDrag(const juce::MouseEvent& e) override;

    private:
        PresetManager& presetManager;
        LayeredSynth&  synth;
        int            layerIndex{ 0 };
        int            activeHandle{ 0 };
    };

    // Animated Waveform Viewer
    class WaveformViewer : public juce::Component {
    public:
        WaveformViewer();
        void setSampleInfo(const juce::String& sampleName, float playheadPos0to1);
        void paint(juce::Graphics& g) override;

    private:
        juce::String currentSampleName{ "No Sample Loaded" };
        float        playheadPosition{ 0.0f };
    };

    struct EditorComponent : public juce::Component {
        AudioEngine&   audioEngine;
        PresetManager& presetManager;
        int            layerIndex{ 0 };

        juce::Label    instrumentLabel{ "InstLabel", "SELECT INSTRUMENT CONTAINER (.BIN)" };
        juce::ComboBox instrumentDropDown;
        juce::Label    syncBadgeLabel;

        juce::Slider   inputGainSlider;
        juce::Label    inputGainLabel{ "InputGain", "Sample Input Gain" };

        AdsrCanvas     adsrCanvas;
        WaveformViewer waveformViewer;

        juce::Slider attackSlider;
        juce::Label  attackLabel{ "Att", "Attack" };
        juce::Slider decaySlider;
        juce::Label  decayLabel{ "Dec", "Decay" };
        juce::Slider sustainSlider;
        juce::Label  sustainLabel{ "Sus", "Sustain" };
        juce::Slider releaseSlider;
        juce::Label  releaseLabel{ "Rel", "Release" };

        EditorComponent(AudioEngine& ae, PresetManager& pm, int layerIndex);
        void resized() override;
        void refreshInstrumentList();
        void updateSyncStatus();

        WaveformViewer& getWaveformViewer() { return waveformViewer; }
    };

    std::unique_ptr<EditorComponent> editorComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InstrumentEditorWindow)
};
