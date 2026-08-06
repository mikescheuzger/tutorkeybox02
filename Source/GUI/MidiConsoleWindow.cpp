#include "MidiConsoleWindow.h"

// =============================================================================
// ConsoleComponent Implementation
// =============================================================================
MidiConsoleWindow::ConsoleComponent::ConsoleComponent(MidiState& /*state*/) {
    logTextEditor.setMultiLine(true);
    logTextEditor.setReadOnly(true);
    logTextEditor.setScrollbarsShown(true);
    logTextEditor.setCaretVisible(false);
    logTextEditor.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain));
    logTextEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff18181b)); // Sleek dark mode
    logTextEditor.setColour(juce::TextEditor::textColourId, juce::Colour(0xff38bdf8));      // Cyan log text

    addAndMakeVisible(logTextEditor);

    clearButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff27272a));
    clearButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    clearButton.onClick = [this] { clearLog(); };
    addAndMakeVisible(clearButton);

    autoScrollToggle.setToggleState(true, juce::dontSendNotification);
    autoScrollToggle.setColour(juce::ToggleButton::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(autoScrollToggle);

    updateLog("=== TutorKeyBox02 MIDI & Sample Console Log Initialized ===");
}

void MidiConsoleWindow::ConsoleComponent::resized() {
    auto bounds = getLocalBounds().reduced(8);
    auto topBar = bounds.removeFromTop(32);

    clearButton.setBounds(topBar.removeFromLeft(100));
    topBar.removeFromLeft(12);
    autoScrollToggle.setBounds(topBar.removeFromLeft(120));

    bounds.removeFromTop(8);
    logTextEditor.setBounds(bounds);
}

void MidiConsoleWindow::ConsoleComponent::updateLog(const juce::String& logLine) {
    juce::String timestamp = juce::Time::getCurrentTime().formatted("[%H:%M:%S.%3ms] ");
    logTextEditor.moveCaretToEnd();
    logTextEditor.insertTextAtCaret(timestamp + logLine + "\n");

    if (autoScrollToggle.getToggleState()) {
        logTextEditor.moveCaretToEnd();
    }
}

void MidiConsoleWindow::ConsoleComponent::clearLog() {
    logTextEditor.clear();
    updateLog("=== Log Cleared ===");
}

// =============================================================================
// MidiConsoleWindow DocumentWindow Implementation
// =============================================================================
MidiConsoleWindow::MidiConsoleWindow(MidiState& stateToMonitor)
    : DocumentWindow("TutorKeyBox MIDI & Sample Console",
                     juce::Colour(0xff09090b),
                     DocumentWindow::allButtons),
      midiState(stateToMonitor) {

    consoleComponent = std::make_unique<ConsoleComponent>(stateToMonitor);
    setContentNonOwned(consoleComponent.get(), true);
    setResizable(true, true);
    setResizeLimits(400, 300, 1000, 800);
    centreWithSize(600, 450);

    midiState.addChangeListener(this);
}

MidiConsoleWindow::~MidiConsoleWindow() {
    midiState.removeChangeListener(this);
}

void MidiConsoleWindow::closeButtonPressed() {
    setVisible(false);
}

void MidiConsoleWindow::changeListenerCallback(juce::ChangeBroadcaster* /*source*/) {
    juce::String sampleName = midiState.getLastSampleName();
    int rootNote = midiState.getLastSampleRoot();
    int keyLow = midiState.getLastSampleKeyLow();
    int keyHigh = midiState.getLastSampleKeyHigh();
    int velLow = midiState.getLastSampleVelLow();
    int velHigh = midiState.getLastSampleVelHigh();

    if (sampleName.isNotEmpty()) {
        juce::String logMsg = "TRIGGERED -> " + sampleName +
                              " [Root: " + juce::String(rootNote) +
                              " (" + juce::MidiMessage::getMidiNoteName(rootNote, true, true, 4) + ")" +
                              ", KeyZone: " + juce::String(keyLow) + "-" + juce::String(keyHigh) +
                              ", VelZone: " + juce::String(velLow) + "-" + juce::String(velHigh) + "]";
        consoleComponent->updateLog(logMsg);
    }
}
