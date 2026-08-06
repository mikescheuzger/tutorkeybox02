#include "MacroPanelComponent.h"

// =============================================================================
// TargetsContainer Implementation
// =============================================================================
MacroPanelComponent::MacroRowComponent::TargetsContainer::TargetsContainer(
    PresetManager& presetMgr, MacroEngine& engine, int slotIdx)
    : presetManager(presetMgr), macroEngine(engine), macroIndex(slotIdx) {

    addAndMakeVisible(addTargetButton);
    addTargetButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff27272a));
    addTargetButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    addTargetButton.onClick = [this] {
        auto& slot = presetManager.getMacroSlotRef(macroIndex);
        MacroTarget newTarget;
        newTarget.targetLayer = 0;
        newTarget.targetParam = TargetParam::OutputVolume;
        newTarget.rangeMin = 0.0f;
        newTarget.rangeMax = 1.0f;
        newTarget.offset = 0.0f;
        slot.targets.push_back(newTarget);
        refreshTargets();
    };

    refreshTargets();
}

void MacroPanelComponent::MacroRowComponent::TargetsContainer::refreshTargets() {
    targetRows.clear();
    const auto& slot = presetManager.getMacroSlot(macroIndex);

    for (size_t i = 0; i < slot.targets.size(); ++i) {
        auto row = std::make_unique<TargetRow>();

        // Layer ComboBox
        row->layerCombo = std::make_unique<juce::ComboBox>();
        row->layerCombo->addItem("Layer 1", 1);
        row->layerCombo->addItem("Layer 2", 2);
        row->layerCombo->addItem("Layer 3", 3);
        row->layerCombo->addItem("Layer 4", 4);
        row->layerCombo->setSelectedId(slot.targets[i].targetLayer + 1, juce::dontSendNotification);
        row->layerCombo->onChange = [this, i] {
            auto& s = presetManager.getMacroSlotRef(macroIndex);
            if (i < s.targets.size()) {
                s.targets[i].targetLayer = targetRows[i]->layerCombo->getSelectedItemIndex();
            }
        };
        addAndMakeVisible(row->layerCombo.get());

        // Parameter ComboBox
        row->paramCombo = std::make_unique<juce::ComboBox>();
        row->paramCombo->addItem("Output Volume", 1);
        row->paramCombo->addItem("Sample Input Gain", 2);
        row->paramCombo->addItem("Filter Cutoff", 3);
        row->paramCombo->addItem("Filter Resonance", 4);
        row->paramCombo->addItem("ADSR Attack", 5);
        row->paramCombo->addItem("ADSR Decay", 6);
        row->paramCombo->addItem("ADSR Sustain", 7);
        row->paramCombo->addItem("ADSR Release", 8);
        row->paramCombo->addItem("AUX Send", 9);
        row->paramCombo->setSelectedId((int)slot.targets[i].targetParam + 1, juce::dontSendNotification);
        row->paramCombo->onChange = [this, i] {
            auto& s = presetManager.getMacroSlotRef(macroIndex);
            if (i < s.targets.size()) {
                s.targets[i].targetParam = (TargetParam)targetRows[i]->paramCombo->getSelectedItemIndex();
            }
        };
        addAndMakeVisible(row->paramCombo.get());

        targetRows.push_back(std::move(row));
    }

    resized();
}

void MacroPanelComponent::MacroRowComponent::TargetsContainer::resized() {
    auto bounds = getLocalBounds();
    for (size_t i = 0; i < targetRows.size(); ++i) {
        auto r = bounds.removeFromTop(24);
        targetRows[i]->layerCombo->setBounds(r.removeFromLeft(75));
        r.removeFromLeft(4);
        targetRows[i]->paramCombo->setBounds(r.removeFromLeft(120));
        bounds.removeFromTop(4);
    }
    addTargetButton.setBounds(bounds.removeFromLeft(28).withHeight(22));
}

// =============================================================================
// MacroRowComponent Implementation
// =============================================================================
MacroPanelComponent::MacroRowComponent::MacroRowComponent(
    PresetManager& presetMgr, MacroEngine& engine, int slotIndex)
    : presetManager(presetMgr), macroEngine(engine), macroIndex(slotIndex),
      targetsContainer(presetMgr, engine, slotIndex) {

    const auto& slot = presetManager.getMacroSlot(slotIndex);

    addAndMakeVisible(macroNameLabel);
    macroNameLabel.setText(slot.name, juce::dontSendNotification);
    macroNameLabel.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    macroNameLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);

    addAndMakeVisible(macroKnob);
    macroKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    macroKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 40, 14);
    macroKnob.setRange(0.0, 1.0, 0.01);
    macroKnob.setValue(0.5);
    macroKnob.onValueChange = [this] {
        macroEngine.setMacroNormalizedValue(macroIndex, (float)macroKnob.getValue());
    };

    addAndMakeVisible(learnButton);
    learnButton.onClick = [this] {
        auto& s = presetManager.getMacroSlotRef(macroIndex);
        s.isLearning = !s.isLearning;
        updateStateFromPreset();
    };

    addAndMakeVisible(reverseToggle);
    reverseToggle.setToggleState(slot.isReversed, juce::dontSendNotification);
    reverseToggle.onClick = [this] {
        auto& s = presetManager.getMacroSlotRef(macroIndex);
        s.isReversed = reverseToggle.getToggleState();
    };

    addAndMakeVisible(targetsContainer);
    updateStateFromPreset();
}

void MacroPanelComponent::MacroRowComponent::updateStateFromPreset() {
    const auto& slot = presetManager.getMacroSlot(macroIndex);

    if (slot.isLearning) {
        learnButton.setColour(juce::TextButton::buttonColourId, juce::Colours::orange);
        learnButton.setButtonText("LEARNING");
    } else if (slot.isMapped) {
        learnButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff10b981)); // Green LED
        learnButton.setButtonText("CC " + juce::String(slot.ccNumber));
    } else {
        learnButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffef4444)); // Red LED (unmapped)
        learnButton.setButtonText("UNMAPPED");
    }
}

void MacroPanelComponent::MacroRowComponent::resized() {
    auto bounds = getLocalBounds().reduced(4);

    // Left Column: Macro Name Label (Top) + Rotary Knob (Bottom)
    auto leftBox = bounds.removeFromLeft(64);
    macroNameLabel.setBounds(leftBox.removeFromTop(16));
    macroKnob.setBounds(leftBox.removeFromTop(44));

    bounds.removeFromLeft(8);

    // Middle Column: Learn Button + Reverse Toggle
    auto ctrlBox = bounds.removeFromLeft(95);
    ctrlBox.removeFromTop(6);
    learnButton.setBounds(ctrlBox.removeFromTop(24));
    ctrlBox.removeFromTop(4);
    reverseToggle.setBounds(ctrlBox.removeFromTop(20));

    bounds.removeFromLeft(12);

    // Right Column: Target Dropdowns Container
    targetsContainer.setBounds(bounds);
}

// =============================================================================
// MacroPanelComponent Implementation
// =============================================================================
MacroPanelComponent::MacroPanelComponent(PresetManager& presetTarget, MacroEngine& engineTarget)
    : presetManager(presetTarget), macroEngine(engineTarget) {

    addAndMakeVisible(headerLabel);
    headerLabel.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    headerLabel.setColour(juce::Label::textColourId, juce::Colour(0xffa855f7)); // Purple

    for (int i = 0; i < PresetManager::NUM_MACROS; ++i) {
        macroRows[i] = std::make_unique<MacroRowComponent>(presetTarget, engineTarget, i);
        addAndMakeVisible(macroRows[i].get());
    }
}

void MacroPanelComponent::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat().reduced(2.0f);
    g.setColour(juce::Colour(0xff18181b));
    g.fillRoundedRectangle(bounds, 8.0f);

    g.setColour(juce::Colour(0xff27272a));
    g.drawRoundedRectangle(bounds, 8.0f, 1.5f);
}

void MacroPanelComponent::resized() {
    auto bounds = getLocalBounds().reduced(8);
    headerLabel.setBounds(bounds.removeFromTop(22));
    bounds.removeFromTop(4);

    int rowHeight = (bounds.getHeight() - 12) / PresetManager::NUM_MACROS;
    for (int i = 0; i < PresetManager::NUM_MACROS; ++i) {
        macroRows[i]->setBounds(bounds.removeFromTop(rowHeight));
        bounds.removeFromTop(4);
    }
}
