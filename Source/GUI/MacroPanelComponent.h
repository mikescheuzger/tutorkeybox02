#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "../Core/MacroEngine.h"
#include "../Core/PresetManager.h"

// =============================================================================
// MacroPanelComponent — 4-Slot Multi-Target Macro Control Panel
//
// Features:
//   • 4 Labeled Macro Knobs
//   • CC-Learn Toggle Button (Red LED = Unmapped, Green LED = Mapped)
//   • CC-Reverse Checkbox (Inverts 0-127 MIDI values)
//   • Target Parameter Dropdown (All layer parameters)
//   • Modulation Range (Min/Max) and Bipolar Offset controls
//   • '+' Button to expand targets list dynamically with scrollbar
// =============================================================================
class MacroPanelComponent : public juce::Component {
public:
    MacroPanelComponent(PresetManager& presetTarget, MacroEngine& engineTarget);
    ~MacroPanelComponent() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    PresetManager& presetManager;
    MacroEngine&   macroEngine;

    juce::Label headerLabel{ "MacroHeader", "MACRO CONTROL PANEL" };

    struct MacroRowComponent : public juce::Component {
        PresetManager& presetManager;
        MacroEngine&   macroEngine;
        int            macroIndex{ 0 };

        juce::Label       macroNameLabel;
        juce::Slider      macroKnob;
        juce::TextButton  learnButton;
        juce::ToggleButton reverseToggle{ "Rev" };

        struct TargetsContainer : public juce::Component {
            PresetManager& presetManager;
            MacroEngine&   macroEngine;
            int            macroIndex{ 0 };

            juce::TextButton addTargetButton{ "+" };

            struct TargetRow {
                std::unique_ptr<juce::ComboBox> layerCombo;
                std::unique_ptr<juce::ComboBox> paramCombo;
            };

            std::vector<std::unique_ptr<TargetRow>> targetRows;

            TargetsContainer(PresetManager& pMgr, MacroEngine& mEngine, int slotIdx);
            void refreshTargets();
            void resized() override;
        };

        TargetsContainer targetsContainer;

        MacroRowComponent(PresetManager& pMgr, MacroEngine& mEngine, int slotIndex);
        void updateStateFromPreset();
        void resized() override;
    };

    std::array<std::unique_ptr<MacroRowComponent>, 4> macroRows;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MacroPanelComponent)
};
