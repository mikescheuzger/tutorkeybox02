#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "../Audio/AudioEngine.h"

// =============================================================================
// MidiConsoleWindow — Detached Real-Time MIDI & Sample Inspector Console
//
// Features:
//   • Opens in a separate floating window when triggered from GUI header button
//   • Displays timestamped log of raw incoming MIDI events (Note-On, Note-Off, CC)
//   • Displays sample inspection metadata (sample name, root note, velocity zone)
//   • Clear Log button & auto-scroll toggle
// =============================================================================
class MidiConsoleWindow : public juce::DocumentWindow,
                           public juce::ChangeListener {
public:
    explicit MidiConsoleWindow(AudioEngine& engineToMonitor);
    ~MidiConsoleWindow() override;

    void closeButtonPressed() override;
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    void logMidiMessage(const juce::MidiMessage& msg);

    class ConsoleComponent : public juce::Component {
    public:
        explicit ConsoleComponent(MidiState& state);
        ~ConsoleComponent() override = default;

        void resized() override;
        void updateLog(const juce::String& logLine);
        void clearLog();

    private:
        juce::TextEditor logTextEditor;
        juce::TextButton clearButton{ "Clear Log" };
        juce::ToggleButton autoScrollToggle{ "Auto-scroll" };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ConsoleComponent)
    };

private:
    AudioEngine& audioEngine;
    std::unique_ptr<ConsoleComponent> consoleComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiConsoleWindow)
};
