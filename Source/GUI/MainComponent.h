#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "../Audio/AudioEngine.h"
#include "../Core/PresetManager.h"
#include "../Core/MacroEngine.h"
#include "DeployControlBar.h"
#include "TelemetryHeaderView.h"
#include "LayerCardComponent.h"
#include "FXCardComponent.h"
#include "MasterChannelStrip.h"
#include "MacroPanelComponent.h"

// =============================================================================
// MainComponent — Main Desktop GUI Application Container Component
//
// Layout Overview:
//   ┌────────────────────────────────────────────────────────────────────────┐
//   │ DeployControlBar (Status Badge, Deploy Button, MIDI Console, Presets)   │
//   ├────────────────────────────────────────────────────────────────────────┤
//   │ TelemetryHeaderView (MacBook CPU/RAM | KeyBox Pi 5 CPU/Voices)          │
//   ├───────────────────────────────────────────────────────┬────────────────┤
//   │ Layer 1 │ Layer 2 │ Layer 3 │ Layer 4 │ FX (Card 5)   │ Master Strip   │
//   │ Card    │ Card    │ Card    │ Card    │ (Delay->Rev)  │ (Comp/Lim/VU)  │
//   ├───────────────────────────────────────────────────────┤                │
//   │ MacroPanelComponent (4 Slots: CC-Learn, Rev, Target+) │                │
//   └───────────────────────────────────────────────────────┴────────────────┘
// =============================================================================
class MainComponent : public juce::Component {
public:
    MainComponent();
    ~MainComponent() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    MidiState          midiState;
    AudioEngine        audioEngine{ midiState };
    PresetManager      presetManager;
    MacroEngine        macroEngine{ presetManager, audioEngine.getSynth() };

    DeployControlBar   controlBar{ audioEngine, presetManager };
    TelemetryHeaderView telemetryView{ audioEngine };

    // 4 Modular Synth Layer Cards
    std::array<std::unique_ptr<LayerCardComponent>, 4> layerCards;

    // 5th Card: Master FX (Delay first, then Reverb)
    FXCardComponent    fxCard{ audioEngine };

    // Macro Panel
    MacroPanelComponent macroPanel{ presetManager, macroEngine };

    // Master Channel Strip (Logic Pro style)
    MasterChannelStrip masterStrip{ audioEngine, presetManager };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
