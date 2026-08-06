#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PresetManager.h"
#include "../Synth/LayeredSynth.h"

// =============================================================================
// MacroEngine — Real-Time MIDI CC Learn & Parameter Modulation Engine
//
// Responsibilities:
//   • Listens to incoming MIDI CC messages
//   • Handles interactive CC-Learn mode (binds next moved physical controller)
//   • Evaluates CC reversal, modulation range, and bipolar offset per target
//   • Dispatches real-time updates to LayeredSynth
// =============================================================================
class MacroEngine {
public:
    MacroEngine(PresetManager& presetMgr, LayeredSynth& synthTarget);
    ~MacroEngine() = default;

    /** Process incoming MIDI message. Intercepts CCs for CC-Learn & macro modulation. */
    void processMidiMessage(const juce::MidiMessage& message);

    /** Manually set normalized macro value (0.0 to 1.0) and dispatch to targets. */
    void setMacroNormalizedValue(int macroIndex, float normalizedVal0to1);

    /** Toggle CC-Learn mode for a specific macro slot (0 to 3). */
    void setCcLearning(int slotIndex, bool isLearning);

    /** Manually bind a CC number to a macro slot. */
    void bindCcToSlot(int slotIndex, int ccNumber);

    /** Add a target modulation binding to a macro slot. */
    void addTargetToSlot(int slotIndex, const MacroTarget& target);

    /** Remove target binding at targetIndex from a macro slot. */
    void removeTargetFromSlot(int slotIndex, int targetIndex);

    /** Clear all target bindings from a macro slot. */
    void clearTargetsFromSlot(int slotIndex);

    /** Apply macro modulation to all bound target parameters. */
    void applyMacroToTargets(int macroIndex, float normalizedVal0to1);

private:
    PresetManager& presetManager;
    LayeredSynth&  synth;

    float applyScalingAndOffset(float val0to1, const MacroTarget& target);
};
