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

    updateLog("=== TutorKeyBox02 Real-Time MIDI & Sample Inspector Console ===");
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
    bool shouldScroll = autoScrollToggle.getToggleState();

    if (shouldScroll) {
        logTextEditor.moveCaretToEnd();
        logTextEditor.insertTextAtCaret(timestamp + logLine + "\n");
        logTextEditor.setCaretPosition(logTextEditor.getText().length());
    } else {
        logTextEditor.setText(logTextEditor.getText() + timestamp + logLine + "\n", false);
    }
}

void MidiConsoleWindow::ConsoleComponent::clearLog() {
    logTextEditor.clear();
    updateLog("=== Log Cleared ===");
}

// =============================================================================
// MidiConsoleWindow DocumentWindow Implementation
// =============================================================================
MidiConsoleWindow::MidiConsoleWindow(AudioEngine& engineToMonitor)
    : DocumentWindow("TutorKeyBox MIDI & Sample Console",
                     juce::Colour(0xff09090b),
                     DocumentWindow::allButtons),
      audioEngine(engineToMonitor) {

    consoleComponent = std::make_unique<ConsoleComponent>(engineToMonitor.getMidiState());
    setContentNonOwned(consoleComponent.get(), true);
    setResizable(true, true);
    setResizeLimits(450, 320, 1100, 850);
    centreWithSize(640, 480);

    audioEngine.getMidiState().addChangeListener(this);
}

MidiConsoleWindow::~MidiConsoleWindow() {
    audioEngine.getMidiState().removeChangeListener(this);
}

void MidiConsoleWindow::closeButtonPressed() {
    setVisible(false);
}

void MidiConsoleWindow::logMidiMessage(const juce::MidiMessage& msg) {
    if (consoleComponent == nullptr) return;

    if (msg.isNoteOn()) {
        int note = msg.getNoteNumber();
        juce::String noteName = juce::MidiMessage::getMidiNoteName(note, true, true, 4);
        int vel = juce::roundToInt(msg.getFloatVelocity() * 100.0f);
        consoleComponent->updateLog("MIDI Note On: " + noteName + " (" + juce::String(note) + ") | Velocity: " + juce::String(vel) + "%");
    } else if (msg.isNoteOff()) {
        int note = msg.getNoteNumber();
        juce::String noteName = juce::MidiMessage::getMidiNoteName(note, true, true, 4);
        consoleComponent->updateLog("MIDI Note Off: " + noteName + " (" + juce::String(note) + ")");
    } else if (msg.isController()) {
        int cc = msg.getControllerNumber();
        int val = msg.getControllerValue();
        juce::String ccName = (cc == 64) ? "Sustain Pedal (CC64)" : ("CC " + juce::String(cc));
        consoleComponent->updateLog("MIDI Controller: " + ccName + " -> Value: " + juce::String(val));
    }
}

void MidiConsoleWindow::changeListenerCallback(juce::ChangeBroadcaster* /*source*/) {
    juce::String sampleName = audioEngine.getMidiState().getLastSampleName();
    int rootNote = audioEngine.getMidiState().getLastSampleRoot();
    int keyLow = audioEngine.getMidiState().getLastSampleKeyLow();
    int keyHigh = audioEngine.getMidiState().getLastSampleKeyHigh();
    int velLow = audioEngine.getMidiState().getLastSampleVelLow();
    int velHigh = audioEngine.getMidiState().getLastSampleVelHigh();

    if (sampleName.isNotEmpty()) {
        juce::String logMsg = "SAMPLE TRIGGER -> " + sampleName +
                              " [Root: " + juce::String(rootNote) +
                              " (" + juce::MidiMessage::getMidiNoteName(rootNote, true, true, 4) + ")" +
                              ", KeyZone: " + juce::String(keyLow) + "-" + juce::String(keyHigh) +
                              ", VelZone: " + juce::String(velLow) + "-" + juce::String(velHigh) + "]";
        consoleComponent->updateLog(logMsg);
    }
}
