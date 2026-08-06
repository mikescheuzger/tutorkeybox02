#include "PresetManager.h"

// =============================================================================
// Constructor — Initialises 4 Layers and 4 Macro Slots with defaults
// =============================================================================
PresetManager::PresetManager() {
    for (int i = 0; i < 4; ++i) {
        layers[i].volume = 1.0f;
        layers[i].muted = false;
        layers[i].sampleInputGain = 1.0f;
        layers[i].auxSendGain = 0.0f;
        layers[i].filterCutoffHz = 20000.0f;
        layers[i].filterResonanceQ = 0.707f;
        layers[i].attackMs = 10.0f;
        layers[i].decayMs = 100.0f;
        layers[i].sustainLevel = 1.0f;
        layers[i].releaseMs = 300.0f;
        layers[i].sampleContainerPath = "";
    }

    for (int i = 0; i < NUM_MACROS; ++i) {
        macros[i].name = "Macro " + juce::String(i + 1);
        macros[i].ccNumber = -1;
        macros[i].isLearning = false;
        macros[i].isMapped = false;
        macros[i].isReversed = false;
        macros[i].targets.clear();
    }
}

// =============================================================================
// MIDI CC Mapping Setters / Getters
// =============================================================================
void PresetManager::setFaderCc(int faderIndex, int ccNumber) {
    if (faderIndex >= 0 && faderIndex < 4) ccMapping.faderCc[faderIndex] = ccNumber;
}

int PresetManager::getFaderCc(int faderIndex) const {
    return (faderIndex >= 0 && faderIndex < 4) ? ccMapping.faderCc[faderIndex] : -1;
}

void PresetManager::setButtonCc(int buttonIndex, int ccNumber) {
    if (buttonIndex >= 0 && buttonIndex < 3) ccMapping.buttonCc[buttonIndex] = ccNumber;
}

int PresetManager::getButtonCc(int buttonIndex) const {
    return (buttonIndex >= 0 && buttonIndex < 3) ? ccMapping.buttonCc[buttonIndex] : -1;
}

void PresetManager::setKnobCc(int knobIndex, int ccNumber) {
    if (knobIndex >= 0 && knobIndex < 2) ccMapping.knobCc[knobIndex] = ccNumber;
}

int PresetManager::getKnobCc(int knobIndex) const {
    return (knobIndex >= 0 && knobIndex < 2) ? ccMapping.knobCc[knobIndex] : -1;
}

// =============================================================================
// Layer Preset Setters / Getters
// =============================================================================
void PresetManager::setLayerPreset(int layerIndex, const LayerPreset& preset) {
    if (layerIndex >= 0 && layerIndex < 4) {
        layers[layerIndex] = preset;
    }
}

const LayerPreset& PresetManager::getLayerPreset(int layerIndex) const {
    static LayerPreset defaultLayer;
    return (layerIndex >= 0 && layerIndex < 4) ? layers[layerIndex] : defaultLayer;
}

LayerPreset& PresetManager::getLayerPresetRef(int layerIndex) {
    static LayerPreset defaultLayer;
    return (layerIndex >= 0 && layerIndex < 4) ? layers[layerIndex] : defaultLayer;
}

// =============================================================================
// MasterChain Preset Setters / Getters
// =============================================================================
void PresetManager::setMasterChainPreset(const MasterChainPreset& preset) {
    masterChain = preset;
}

const MasterChainPreset& PresetManager::getMasterChainPreset() const {
    return masterChain;
}

// =============================================================================
// MacroSlot Setters / Getters
// =============================================================================
void PresetManager::setMacroSlot(int macroIndex, const MacroSlot& slot) {
    if (macroIndex >= 0 && macroIndex < NUM_MACROS) {
        macros[macroIndex] = slot;
    }
}

const MacroSlot& PresetManager::getMacroSlot(int macroIndex) const {
    static MacroSlot defaultSlot;
    return (macroIndex >= 0 && macroIndex < NUM_MACROS) ? macros[macroIndex] : defaultSlot;
}

MacroSlot& PresetManager::getMacroSlotRef(int macroIndex) {
    static MacroSlot defaultSlot;
    return (macroIndex >= 0 && macroIndex < NUM_MACROS) ? macros[macroIndex] : defaultSlot;
}

// =============================================================================
// JSON Serialization (toJsonString)
// =============================================================================
juce::String PresetManager::toJsonString() const {
    auto root = std::make_unique<juce::DynamicObject>();

    // 1. CC Mapping
    auto ccObj = std::make_unique<juce::DynamicObject>();
    juce::Array<juce::var> fadersArr, buttonsArr, knobsArr;
    for (int i = 0; i < 4; ++i) fadersArr.add(ccMapping.faderCc[i]);
    for (int i = 0; i < 3; ++i) buttonsArr.add(ccMapping.buttonCc[i]);
    for (int i = 0; i < 2; ++i) knobsArr.add(ccMapping.knobCc[i]);

    ccObj->setProperty("faders", fadersArr);
    ccObj->setProperty("buttons", buttonsArr);
    ccObj->setProperty("knobs", knobsArr);
    ccObj->setProperty("sustainPedal", ccMapping.sustainPedalCc);

    root->setProperty("ccMapping", ccObj.release());

    // 2. Layers
    juce::Array<juce::var> layersArr;
    for (int i = 0; i < 4; ++i) {
        auto layerObj = std::make_unique<juce::DynamicObject>();
        layerObj->setProperty("volume", layers[i].volume);
        layerObj->setProperty("muted", layers[i].muted);
        layerObj->setProperty("sampleInputGain", layers[i].sampleInputGain);
        layerObj->setProperty("auxSendGain", layers[i].auxSendGain);
        layerObj->setProperty("filterCutoffHz", layers[i].filterCutoffHz);
        layerObj->setProperty("filterResonanceQ", layers[i].filterResonanceQ);
        layerObj->setProperty("attackMs", layers[i].attackMs);
        layerObj->setProperty("decayMs", layers[i].decayMs);
        layerObj->setProperty("sustainLevel", layers[i].sustainLevel);
        layerObj->setProperty("releaseMs", layers[i].releaseMs);
        layerObj->setProperty("containerPath", layers[i].sampleContainerPath);
        layersArr.add(layerObj.release());
    }

    root->setProperty("layers", layersArr);

    // 3. MasterChain
    auto mcObj = std::make_unique<juce::DynamicObject>();
    mcObj->setProperty("compEnabled", masterChain.compEnabled);
    mcObj->setProperty("compThresholdDb", masterChain.compThresholdDb);
    mcObj->setProperty("compRatio", masterChain.compRatio);
    mcObj->setProperty("compAttackMs", masterChain.compAttackMs);
    mcObj->setProperty("compReleaseMs", masterChain.compReleaseMs);

    mcObj->setProperty("limEnabled", masterChain.limEnabled);
    mcObj->setProperty("limThresholdDb", masterChain.limThresholdDb);

    mcObj->setProperty("clipEnabled", masterChain.clipEnabled);
    mcObj->setProperty("clipThresholdDb", masterChain.clipThresholdDb);
    mcObj->setProperty("clipDriveDb", masterChain.clipDriveDb);

    mcObj->setProperty("masterGain", masterChain.masterGain);

    root->setProperty("masterChain", mcObj.release());

    // 4. Macros
    juce::Array<juce::var> macrosArr;
    for (int i = 0; i < NUM_MACROS; ++i) {
        auto macroObj = std::make_unique<juce::DynamicObject>();
        macroObj->setProperty("name", macros[i].name);
        macroObj->setProperty("ccNumber", macros[i].ccNumber);
        macroObj->setProperty("isLearning", macros[i].isLearning);
        macroObj->setProperty("isMapped", macros[i].isMapped);
        macroObj->setProperty("isReversed", macros[i].isReversed);

        juce::Array<juce::var> targetsArr;
        for (const auto& t : macros[i].targets) {
            auto tObj = std::make_unique<juce::DynamicObject>();
            tObj->setProperty("targetLayer", t.targetLayer);
            tObj->setProperty("targetParam", (int)t.targetParam);
            tObj->setProperty("rangeMin", t.rangeMin);
            tObj->setProperty("rangeMax", t.rangeMax);
            tObj->setProperty("offset", t.offset);
            targetsArr.add(tObj.release());
        }
        macroObj->setProperty("targets", targetsArr);
        macrosArr.add(macroObj.release());
    }

    root->setProperty("macros", macrosArr);

    return juce::JSON::toString(juce::var(root.release()));
}

// =============================================================================
// JSON Deserialization (loadFromJsonString)
// =============================================================================
bool PresetManager::loadFromJsonString(const juce::String& jsonText) {
    auto parsed = juce::JSON::parse(jsonText);
    if (!parsed.isObject()) return false;

    auto* root = parsed.getDynamicObject();
    if (root == nullptr) return false;

    // 1. CC Mapping
    if (root->hasProperty("ccMapping")) {
        if (auto* ccObj = root->getProperty("ccMapping").getDynamicObject()) {
            if (auto* faders = ccObj->getProperty("faders").getArray()) {
                for (int i = 0; i < juce::jmin(4, faders->size()); ++i)
                    ccMapping.faderCc[i] = (int)(*faders)[i];
            }
            if (auto* buttons = ccObj->getProperty("buttons").getArray()) {
                for (int i = 0; i < juce::jmin(3, buttons->size()); ++i)
                    ccMapping.buttonCc[i] = (int)(*buttons)[i];
            }
            if (auto* knobs = ccObj->getProperty("knobs").getArray()) {
                for (int i = 0; i < juce::jmin(2, knobs->size()); ++i)
                    ccMapping.knobCc[i] = (int)(*knobs)[i];
            }
            if (ccObj->hasProperty("sustainPedal")) {
                ccMapping.sustainPedalCc = (int)ccObj->getProperty("sustainPedal");
            }
        }
    }

    // 2. Layers
    if (root->hasProperty("layers")) {
        if (auto* layersList = root->getProperty("layers").getArray()) {
            for (int i = 0; i < juce::jmin(4, layersList->size()); ++i) {
                if (auto* lObj = (*layersList)[i].getDynamicObject()) {
                    if (lObj->hasProperty("volume")) layers[i].volume = (float)lObj->getProperty("volume");
                    if (lObj->hasProperty("muted"))  layers[i].muted = (bool)lObj->getProperty("muted");
                    if (lObj->hasProperty("sampleInputGain")) layers[i].sampleInputGain = (float)lObj->getProperty("sampleInputGain");
                    if (lObj->hasProperty("auxSendGain")) layers[i].auxSendGain = (float)lObj->getProperty("auxSendGain");
                    if (lObj->hasProperty("filterCutoffHz")) layers[i].filterCutoffHz = (float)lObj->getProperty("filterCutoffHz");
                    if (lObj->hasProperty("filterResonanceQ")) layers[i].filterResonanceQ = (float)lObj->getProperty("filterResonanceQ");
                    if (lObj->hasProperty("attackMs")) layers[i].attackMs = (float)lObj->getProperty("attackMs");
                    if (lObj->hasProperty("decayMs")) layers[i].decayMs = (float)lObj->getProperty("decayMs");
                    if (lObj->hasProperty("sustainLevel")) layers[i].sustainLevel = (float)lObj->getProperty("sustainLevel");
                    if (lObj->hasProperty("releaseMs")) layers[i].releaseMs = (float)lObj->getProperty("releaseMs");
                    if (lObj->hasProperty("containerPath")) layers[i].sampleContainerPath = lObj->getProperty("containerPath").toString();
                }
            }
        }
    }

    // 3. MasterChain
    if (root->hasProperty("masterChain")) {
        if (auto* mcObj = root->getProperty("masterChain").getDynamicObject()) {
            if (mcObj->hasProperty("compEnabled")) masterChain.compEnabled = (bool)mcObj->getProperty("compEnabled");
            if (mcObj->hasProperty("compThresholdDb")) masterChain.compThresholdDb = (float)mcObj->getProperty("compThresholdDb");
            if (mcObj->hasProperty("compRatio")) masterChain.compRatio = (float)mcObj->getProperty("compRatio");
            if (mcObj->hasProperty("compAttackMs")) masterChain.compAttackMs = (float)mcObj->getProperty("compAttackMs");
            if (mcObj->hasProperty("compReleaseMs")) masterChain.compReleaseMs = (float)mcObj->getProperty("compReleaseMs");

            if (mcObj->hasProperty("limEnabled")) masterChain.limEnabled = (bool)mcObj->getProperty("limEnabled");
            if (mcObj->hasProperty("limThresholdDb")) masterChain.limThresholdDb = (float)mcObj->getProperty("limThresholdDb");

            if (mcObj->hasProperty("clipEnabled")) masterChain.clipEnabled = (bool)mcObj->getProperty("clipEnabled");
            if (mcObj->hasProperty("clipThresholdDb")) masterChain.clipThresholdDb = (float)mcObj->getProperty("clipThresholdDb");
            if (mcObj->hasProperty("clipDriveDb")) masterChain.clipDriveDb = (float)mcObj->getProperty("clipDriveDb");

            if (mcObj->hasProperty("masterGain")) masterChain.masterGain = (float)mcObj->getProperty("masterGain");
        }
    }

    // 4. Macros
    if (root->hasProperty("macros")) {
        if (auto* macrosList = root->getProperty("macros").getArray()) {
            for (int i = 0; i < juce::jmin(NUM_MACROS, macrosList->size()); ++i) {
                if (auto* mObj = (*macrosList)[i].getDynamicObject()) {
                    if (mObj->hasProperty("name")) macros[i].name = mObj->getProperty("name").toString();
                    if (mObj->hasProperty("ccNumber")) macros[i].ccNumber = (int)mObj->getProperty("ccNumber");
                    if (mObj->hasProperty("isLearning")) macros[i].isLearning = (bool)mObj->getProperty("isLearning");
                    if (mObj->hasProperty("isMapped")) macros[i].isMapped = (bool)mObj->getProperty("isMapped");
                    if (mObj->hasProperty("isReversed")) macros[i].isReversed = (bool)mObj->getProperty("isReversed");

                    macros[i].targets.clear();
                    if (mObj->hasProperty("targets")) {
                        if (auto* targetsList = mObj->getProperty("targets").getArray()) {
                            for (int tIdx = 0; tIdx < targetsList->size(); ++tIdx) {
                                if (auto* tObj = (*targetsList)[tIdx].getDynamicObject()) {
                                    MacroTarget t;
                                    t.targetLayer = (int)tObj->getProperty("targetLayer");
                                    t.targetParam = (TargetParam)(int)tObj->getProperty("targetParam");
                                    t.rangeMin = (float)tObj->getProperty("rangeMin");
                                    t.rangeMax = (float)tObj->getProperty("rangeMax");
                                    t.offset = (float)tObj->getProperty("offset");
                                    macros[i].targets.push_back(t);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return true;
}

bool PresetManager::saveToFile(const juce::File& file) const {
    return file.replaceWithText(toJsonString());
}

bool PresetManager::loadFromFile(const juce::File& file) {
    if (!file.existsAsFile()) return false;
    return loadFromJsonString(file.loadFileAsString());
}
