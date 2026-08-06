#include "MacroEngine.h"

// =============================================================================
// Constructor — Initialises MacroEngine with PresetManager and Synth references
// =============================================================================
MacroEngine::MacroEngine(PresetManager& presetMgr, LayeredSynth& synthTarget)
    : presetManager(presetMgr), synth(synthTarget) {}

// =============================================================================
// processMidiMessage — Intercepts incoming CC messages for Learn & Modulation
// =============================================================================
void MacroEngine::processMidiMessage(const juce::MidiMessage& message) {
    if (!message.isController())
        return;

    int ccNum = message.getControllerNumber();
    int ccVal = message.getControllerValue();
    float normalizedVal = (float)ccVal / 127.0f;

    // ── 1. CC-Learn Mode Intercept ───────────────────────────────────────────
    for (int i = 0; i < PresetManager::NUM_MACROS; ++i) {
        auto& slot = presetManager.getMacroSlotRef(i);
        if (slot.isLearning) {
            slot.ccNumber = ccNum;
            slot.isLearning = false;
            slot.isMapped = true;
            juce::Logger::writeToLog("MacroEngine: Bound Macro " + juce::String(i + 1) +
                                     " (" + slot.name + ") to MIDI CC " + juce::String(ccNum));

            applyMacroToTargets(i, normalizedVal);
            return;
        }
    }

    // ── 2. Dispatch Active CC Modulation ─────────────────────────────────────
    for (int i = 0; i < PresetManager::NUM_MACROS; ++i) {
        const auto& slot = presetManager.getMacroSlot(i);
        if (slot.isMapped && slot.ccNumber == ccNum) {
            applyMacroToTargets(i, normalizedVal);
        }
    }
}

// =============================================================================
// setMacroNormalizedValue — Manual GUI Knob Drag Trigger
// =============================================================================
void MacroEngine::setMacroNormalizedValue(int macroIndex, float normalizedVal0to1) {
    if (macroIndex >= 0 && macroIndex < PresetManager::NUM_MACROS) {
        applyMacroToTargets(macroIndex, juce::jlimit(0.0f, 1.0f, normalizedVal0to1));
    }
}

// =============================================================================
// applyMacroToTargets — Evaluates Range, Offset, and Updates Synth Parameters
// =============================================================================
void MacroEngine::applyMacroToTargets(int macroIndex, float normalizedVal0to1) {
    const auto& slot = presetManager.getMacroSlot(macroIndex);
    float inputVal = slot.isReversed ? (1.0f - normalizedVal0to1) : normalizedVal0to1;

    for (const auto& target : slot.targets) {
        int layerIdx = juce::jlimit(0, LayeredSynth::NUM_LAYERS - 1, target.targetLayer);

        float range = target.rangeMax - target.rangeMin;
        float modVal = juce::jlimit(0.0f, 1.0f, target.rangeMin + (inputVal * range) + target.offset);

        // Fetch layer's current preset state
        auto layerPreset = presetManager.getLayerPreset(layerIdx);

        switch (target.targetParam) {

            case TargetParam::SampleInputGain:
                synth.setLayerSampleInputGain(layerIdx, modVal);
                layerPreset.sampleInputGain = modVal;
                break;

            case TargetParam::OutputVolume:
                synth.setLayerVolume(layerIdx, modVal);
                layerPreset.volume = modVal;
                break;

            case TargetParam::FilterCutoff: {
                // Exponential mapping: 20 Hz to 20,000 Hz
                float cutoffHz = 20.0f * std::pow(1000.0f, modVal);
                synth.setLayerFilterCutoff(layerIdx, cutoffHz);
                layerPreset.filterCutoffHz = cutoffHz;
                break;
            }

            case TargetParam::FilterResonance: {
                // Linear mapping: 0.1 to 10.0
                float resQ = juce::jmap(modVal, 0.1f, 10.0f);
                synth.setLayerFilterResonance(layerIdx, resQ);
                layerPreset.filterResonanceQ = resQ;
                break;
            }

            case TargetParam::AdsrAttack: {
                // Exponential mapping: 0.1 ms to 5000 ms
                float attackMs = 0.1f * std::pow(50000.0f, modVal);
                layerPreset.attackMs = attackMs;
                synth.setLayerAdsr(layerIdx, { layerPreset.attackMs, layerPreset.decayMs,
                                               layerPreset.sustainLevel, layerPreset.releaseMs });
                break;
            }

            case TargetParam::AdsrDecay: {
                // Exponential mapping: 1.0 ms to 10,000 ms
                float decayMs = 1.0f * std::pow(10000.0f, modVal);
                layerPreset.decayMs = decayMs;
                synth.setLayerAdsr(layerIdx, { layerPreset.attackMs, layerPreset.decayMs,
                                               layerPreset.sustainLevel, layerPreset.releaseMs });
                break;
            }

            case TargetParam::AdsrSustain: {
                layerPreset.sustainLevel = modVal;
                synth.setLayerAdsr(layerIdx, { layerPreset.attackMs, layerPreset.decayMs,
                                               layerPreset.sustainLevel, layerPreset.releaseMs });
                break;
            }

            case TargetParam::AdsrRelease: {
                // Exponential mapping: 1.0 ms to 10,000 ms
                float releaseMs = 1.0f * std::pow(10000.0f, modVal);
                layerPreset.releaseMs = releaseMs;
                synth.setLayerAdsr(layerIdx, { layerPreset.attackMs, layerPreset.decayMs,
                                               layerPreset.sustainLevel, layerPreset.releaseMs });
                break;
            }

            case TargetParam::AuxSend:
                synth.setLayerAuxSend(layerIdx, modVal);
                layerPreset.auxSendGain = modVal;
                break;
        }

        // Keep PresetManager updated with live macro modulated values
        presetManager.setLayerPreset(layerIdx, layerPreset);
    }
}
