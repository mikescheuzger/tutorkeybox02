#pragma once
#include <juce_core/juce_core.h>
#include <array>
#include <vector>

// =============================================================================
// MIDI CC Mappings (Hardware Faders, Buttons, Knobs)
// =============================================================================
struct MidiCcMapping {
    int faderCc[4]{ 1, 2, 3, 4 };   // Default CCs for Faders 1-4
    int buttonCc[3]{ 5, 6, 7 };   // Default CCs for Buttons 1-3
    int knobCc[2]{ 8, 9 };        // Default CCs for Knobs 1-2
    int sustainPedalCc{ 64 };     // Default Sustain Pedal CC
};

// =============================================================================
// LayerPreset — Configuration for one of the 4 Synth Layers
// =============================================================================
struct LayerPreset {
    float volume{ 1.0f };              // Output volume (channel card fader)
    bool  muted{ false };
    float sampleInputGain{ 1.0f };     // Raw sample input volume (Instrument Editor)
    float auxSendGain{ 0.0f };         // AUX send gain to FX Channel

    // Filter parameters
    float filterCutoffHz{ 20000.0f };  // Cutoff (20 Hz - 20000 Hz)
    float filterResonanceQ{ 0.707f };  // Resonance Q (0.1 - 10.0)

    // ADSR parameters (milliseconds & level)
    float attackMs{ 10.0f };           // Attack time (ms)
    float decayMs{ 100.0f };           // Decay time (ms)
    float sustainLevel{ 1.0f };        // Sustain level (0.0 - 1.0)
    float releaseMs{ 300.0f };         // Release time (ms)

    juce::String sampleContainerPath;
};

// =============================================================================
// MasterChainPreset — Configuration for Master Audio Plugin Chain
// =============================================================================
struct MasterChainPreset {
    // Compressor
    bool  compEnabled{ true };
    float compThresholdDb{ -12.0f };
    float compRatio{ 3.0f };
    float compAttackMs{ 15.0f };
    float compReleaseMs{ 100.0f };

    // Limiter
    bool  limEnabled{ true };
    float limThresholdDb{ -0.3f };

    // Clipper
    bool  clipEnabled{ true };
    float clipThresholdDb{ -0.1f };
    float clipDriveDb{ 0.0f };

    // Master Output Gain
    float masterGain{ 1.0f };
};

// =============================================================================
// Macro Target Parameters & Macro Slot Data Models
// =============================================================================
enum class TargetParam {
    SampleInputGain,
    OutputVolume,
    FilterCutoff,
    FilterResonance,
    AdsrAttack,
    AdsrDecay,
    AdsrSustain,
    AdsrRelease,
    AuxSend
};

struct MacroTarget {
    int         targetLayer{ 0 };                 // Layer 0 - 3
    TargetParam targetParam{ TargetParam::OutputVolume };
    float       rangeMin{ 0.0f };                 // Modulation lower bound
    float       rangeMax{ 1.0f };                 // Modulation upper bound
    float       offset{ 0.0f };                   // Center offset shift (-1.0 to +1.0)
};

struct MacroSlot {
    juce::String name{ "Macro 1" };
    int          ccNumber{ -1 };                  // Bound MIDI CC (-1 = unmapped)
    bool         isLearning{ false };             // Active CC-Learn toggle
    bool         isMapped{ false };               // Green/Red mapped status indicator
    bool         isReversed{ false };             // Invert incoming CC (127 - CC)
    std::vector<MacroTarget> targets;            // Multi-target modulation vector
};

#include <juce_events/juce_events.h>

// =============================================================================
// PresetManager — Manages Presets, Serialization & File I/O
// =============================================================================
class PresetManager : public juce::ChangeBroadcaster {
public:
    PresetManager();
    ~PresetManager() = default;

    // MIDI CC Mapping Accessors
    void setFaderCc(int faderIndex, int ccNumber);
    int getFaderCc(int faderIndex) const;

    void setButtonCc(int buttonIndex, int ccNumber);
    int getButtonCc(int buttonIndex) const;

    void setKnobCc(int knobIndex, int ccNumber);
    int getKnobCc(int knobIndex) const;

    // Layer Preset Accessors
    void setLayerPreset(int layerIndex, const LayerPreset& preset);
    const LayerPreset& getLayerPreset(int layerIndex) const;
    LayerPreset& getLayerPresetRef(int layerIndex);

    // MasterChain Preset Accessors
    void setMasterChainPreset(const MasterChainPreset& preset);
    const MasterChainPreset& getMasterChainPreset() const;

    // MacroSlot Accessors
    static constexpr int NUM_MACROS = 4;
    void setMacroSlot(int macroIndex, const MacroSlot& slot);
    const MacroSlot& getMacroSlot(int macroIndex) const;
    MacroSlot& getMacroSlotRef(int macroIndex);

    // JSON Serialization & File I/O
    juce::String toJsonString() const;
    bool loadFromJsonString(const juce::String& jsonText);

    bool saveToFile(const juce::File& file) const;
    bool loadFromFile(const juce::File& file);

private:
    MidiCcMapping ccMapping;
    std::array<LayerPreset, 4> layers;
    MasterChainPreset masterChain;
    std::array<MacroSlot, NUM_MACROS> macros;
};
