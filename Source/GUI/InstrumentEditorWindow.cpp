#include "InstrumentEditorWindow.h"

// =============================================================================
// AdsrCanvas Implementation — Interactive Draggable ADSR Curve
// =============================================================================
InstrumentEditorWindow::AdsrCanvas::AdsrCanvas(PresetManager& presetMgr, LayeredSynth& synthRef, int layerIdx)
    : presetManager(presetMgr), synth(synthRef), layerIndex(layerIdx) {
    setRepaintsOnMouseActivity(true);
}

void InstrumentEditorWindow::AdsrCanvas::updateFromPreset() {
    repaint();
}

void InstrumentEditorWindow::AdsrCanvas::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat().reduced(12.0f);
    g.setColour(juce::Colour(0xff18181b)); // Dark background
    g.fillRoundedRectangle(bounds, 6.0f);

    g.setColour(juce::Colour(0xff27272a));
    g.drawRoundedRectangle(bounds, 6.0f, 1.0f);

    const auto& layer = presetManager.getLayerPreset(layerIndex);

    float totalWidth = bounds.getWidth();
    float height = bounds.getHeight();

    // Map ADSR parameters to canvas X/Y coordinates
    float aWidth = juce::jlimit(10.0f, totalWidth * 0.25f, (layer.attackMs / 5000.0f) * totalWidth * 0.25f);
    float dWidth = juce::jlimit(10.0f, totalWidth * 0.25f, (layer.decayMs / 10000.0f) * totalWidth * 0.25f);
    float rWidth = juce::jlimit(10.0f, totalWidth * 0.25f, (layer.releaseMs / 10000.0f) * totalWidth * 0.25f);
    float sWidth = juce::jmax(20.0f, totalWidth - (aWidth + dWidth + rWidth));

    float sHeight = bounds.getBottom() - (layer.sustainLevel * height * 0.85f);

    juce::Point<float> pStart(bounds.getX(), bounds.getBottom());
    juce::Point<float> pAttack(bounds.getX() + aWidth, bounds.getY() + height * 0.15f);
    juce::Point<float> pDecay(pAttack.x + dWidth, sHeight);
    juce::Point<float> pSustain(pDecay.x + sWidth, sHeight);
    juce::Point<float> pRelease(pSustain.x + rWidth, bounds.getBottom());

    // Draw ADSR Curve Path
    juce::Path path;
    path.startNewSubPath(pStart);
    path.lineTo(pAttack);
    path.lineTo(pDecay);
    path.lineTo(pSustain);
    path.lineTo(pRelease);

    // Gradient fill under path
    juce::ColourGradient fillGrad(juce::Colour(0x6038bdf8), 0, bounds.getY(),
                                  juce::Colour(0x050284c7), 0, bounds.getBottom(), false);
    g.setGradientFill(fillGrad);
    juce::Path strokePath(path);
    strokePath.lineTo(bounds.getRight(), bounds.getBottom());
    strokePath.lineTo(bounds.getX(), bounds.getBottom());
    strokePath.closeSubPath();
    g.fillPath(strokePath);

    // Draw stroke
    g.setColour(juce::Colour(0xff38bdf8)); // Cyan stroke
    g.strokePath(path, juce::PathStrokeType(2.5f));

    // Draw interactive handles
    g.setColour(juce::Colours::white);
    g.fillEllipse(pAttack.x - 5.0f, pAttack.y - 5.0f, 10.0f, 10.0f);
    g.fillEllipse(pDecay.x - 5.0f, pDecay.y - 5.0f, 10.0f, 10.0f);
    g.fillEllipse(pRelease.x - 5.0f, pRelease.y - 5.0f, 10.0f, 10.0f);
}

void InstrumentEditorWindow::AdsrCanvas::resized() {}

void InstrumentEditorWindow::AdsrCanvas::mouseDown(const juce::MouseEvent& e) {
    auto pos = e.position;
    auto bounds = getLocalBounds().toFloat().reduced(12.0f);
    float width = bounds.getWidth();

    const auto& layer = presetManager.getLayerPreset(layerIndex);
    float aWidth = (layer.attackMs / 5000.0f) * width * 0.25f;
    float pAttackX = bounds.getX() + aWidth;

    if (std::abs(pos.x - pAttackX) < 20.0f) activeHandle = 1;
    else if (pos.x > pAttackX && pos.x < pAttackX + width * 0.5f) activeHandle = 2;
    else activeHandle = 3;
}

void InstrumentEditorWindow::AdsrCanvas::mouseDrag(const juce::MouseEvent& e) {
    auto bounds = getLocalBounds().toFloat().reduced(12.0f);
    auto pos = e.position;

    auto layer = presetManager.getLayerPreset(layerIndex);

    if (activeHandle == 1) { // Attack
        float normA = juce::jlimit(0.001f, 1.0f, (pos.x - bounds.getX()) / (bounds.getWidth() * 0.25f));
        layer.attackMs = normA * 5000.0f;
    } else if (activeHandle == 2) { // Decay / Sustain
        float normS = juce::jlimit(0.0f, 1.0f, (bounds.getBottom() - pos.y) / (bounds.getHeight() * 0.85f));
        layer.sustainLevel = normS;
    } else if (activeHandle == 3) { // Release
        float normR = juce::jlimit(0.01f, 1.0f, (bounds.getRight() - pos.x) / (bounds.getWidth() * 0.25f));
        layer.releaseMs = normR * 10000.0f;
    }

    presetManager.setLayerPreset(layerIndex, layer);
    synth.setLayerAdsr(layerIndex, { layer.attackMs, layer.decayMs, layer.sustainLevel, layer.releaseMs });
    repaint();
}

// =============================================================================
// WaveformViewer Implementation — Waveform Display & Animated Playhead
// =============================================================================
InstrumentEditorWindow::WaveformViewer::WaveformViewer() {}

void InstrumentEditorWindow::WaveformViewer::setSampleInfo(const juce::String& name, float playheadPos0to1) {
    currentSampleName = name;
    playheadPosition = playheadPos0to1;
    repaint();
}

void InstrumentEditorWindow::WaveformViewer::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat().reduced(8.0f);
    g.setColour(juce::Colour(0xff18181b));
    g.fillRoundedRectangle(bounds, 6.0f);

    g.setColour(juce::Colour(0xff27272a));
    g.drawRoundedRectangle(bounds, 6.0f, 1.0f);

    // Mock representation of sample waveform
    g.setColour(juce::Colour(0xff0284c7));
    float midY = bounds.getCentreY();
    juce::Path wavePath;
    wavePath.startNewSubPath(bounds.getX(), midY);

    int numPoints = (int)bounds.getWidth() / 4;
    for (int i = 0; i < numPoints; ++i) {
        float x = bounds.getX() + i * 4.0f;
        float env = std::exp(-i * 0.05f); // Natural piano decay visual
        float y = midY + (std::sin(i * 0.4f) * bounds.getHeight() * 0.35f * env);
        wavePath.lineTo(x, y);
    }
    g.strokePath(wavePath, juce::PathStrokeType(1.5f));

    // Sample Title Label
    g.setFont(juce::FontOptions(12.0f, juce::Font::plain));
    g.setColour(juce::Colours::lightgrey);
    g.drawText("ACTIVE SAMPLE: " + currentSampleName, bounds.reduced(10), juce::Justification::topLeft, true);

    // Animated Playhead Line
    if (playheadPosition > 0.0f && playheadPosition < 1.0f) {
        float px = bounds.getX() + (playheadPosition * bounds.getWidth());
        g.setColour(juce::Colours::yellow);
        g.drawVerticalLine((int)px, bounds.getY(), bounds.getBottom());
    }
}

// =============================================================================
// EditorComponent Implementation
// =============================================================================
InstrumentEditorWindow::EditorComponent::EditorComponent(AudioEngine& engine, PresetManager& presetMgr, int layerIdx)
    : audioEngine(engine), presetManager(presetMgr), layerIndex(layerIdx),
      adsrCanvas(presetMgr, engine.getSynth(), layerIdx) {

    addAndMakeVisible(instrumentLabel);
    instrumentLabel.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    instrumentLabel.setColour(juce::Label::textColourId, juce::Colours::grey);

    addAndMakeVisible(instrumentDropDown);
    refreshInstrumentList();

    instrumentDropDown.onChange = [this] {
        juce::String selectedName = instrumentDropDown.getItemText(instrumentDropDown.getSelectedItemIndex());
        if (selectedName.isNotEmpty()) {
            auto layer = presetManager.getLayerPreset(layerIndex);
            layer.sampleContainerPath = selectedName;
            presetManager.setLayerPreset(layerIndex, layer);

            juce::File binFile = juce::File::getCurrentWorkingDirectory().getChildFile(selectedName);
            if (binFile.existsAsFile()) {
                SampleContainerReader::loadContainerFile(binFile, audioEngine.getSynth(), layerIndex);
            }
            updateSyncStatus();
        }
    };

    addAndMakeVisible(syncBadgeLabel);
    syncBadgeLabel.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    updateSyncStatus();

    addAndMakeVisible(waveformViewer);
    addAndMakeVisible(adsrCanvas);

    // Sample Input Gain Slider
    addAndMakeVisible(inputGainSlider);
    inputGainSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    inputGainSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 18);
    inputGainSlider.setRange(0.0, 2.0, 0.01);
    inputGainSlider.setValue(presetManager.getLayerPreset(layerIndex).sampleInputGain);
    inputGainSlider.onValueChange = [this] {
        float val = (float)inputGainSlider.getValue();
        auto layer = presetManager.getLayerPreset(layerIndex);
        layer.sampleInputGain = val;
        presetManager.setLayerPreset(layerIndex, layer);
        audioEngine.getSynth().setLayerSampleInputGain(layerIndex, val);
    };

    addAndMakeVisible(inputGainLabel);
    inputGainLabel.setFont(juce::FontOptions(10.0f, juce::Font::plain));
    inputGainLabel.setJustificationType(juce::Justification::centred);

    // ADSR Sliders
    auto setupSlider = [this](juce::Slider& s, juce::Label& l, double minV, double maxV, double defV) {
        addAndMakeVisible(s);
        s.setSliderStyle(juce::Slider::LinearVertical);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 45, 16);
        s.setRange(minV, maxV, 0.1);
        s.setValue(defV);
        s.onValueChange = [this] {
            auto layer = presetManager.getLayerPreset(layerIndex);
            layer.attackMs = (float)attackSlider.getValue();
            layer.decayMs = (float)decaySlider.getValue();
            layer.sustainLevel = (float)sustainSlider.getValue();
            layer.releaseMs = (float)releaseSlider.getValue();
            presetManager.setLayerPreset(layerIndex, layer);
            audioEngine.getSynth().setLayerAdsr(layerIndex, { layer.attackMs, layer.decayMs, layer.sustainLevel, layer.releaseMs });
            adsrCanvas.updateFromPreset();
        };

        addAndMakeVisible(l);
        l.setFont(juce::FontOptions(10.0f, juce::Font::plain));
        l.setJustificationType(juce::Justification::centred);
    };

    const auto& lp = presetManager.getLayerPreset(layerIndex);
    setupSlider(attackSlider, attackLabel, 0.1, 5000.0, lp.attackMs);
    setupSlider(decaySlider, decayLabel, 1.0, 10000.0, lp.decayMs);
    setupSlider(sustainSlider, sustainLabel, 0.0, 1.0, lp.sustainLevel);
    setupSlider(releaseSlider, releaseLabel, 1.0, 10000.0, lp.releaseMs);
}

void InstrumentEditorWindow::EditorComponent::refreshInstrumentList() {
    instrumentDropDown.clear();

    // Scan for available .bin container files
    juce::File currentDir = juce::File::getCurrentWorkingDirectory();
    auto binFiles = currentDir.findChildFiles(juce::File::findFiles, false, "*.bin");

    int id = 1;
    for (const auto& file : binFiles) {
        instrumentDropDown.addItem(file.getFileName(), id++);
    }

    if (id == 1) {
        instrumentDropDown.addItem("NeumannM49.bin", 1);
        instrumentDropDown.addItem("SalamanderGrand.bin", 2);
    }
    instrumentDropDown.setSelectedId(1, juce::dontSendNotification);
}

void InstrumentEditorWindow::EditorComponent::updateSyncStatus() {
    bool isConnectedToPi = false; // Mock connection status
    if (isConnectedToPi) {
        syncBadgeLabel.setText("MAC & KEYBOX SYNCED", juce::dontSendNotification);
        syncBadgeLabel.setColour(juce::Label::textColourId, juce::Colours::lightgreen);
    } else {
        syncBadgeLabel.setText("MAC STANDALONE ENGINE", juce::dontSendNotification);
        syncBadgeLabel.setColour(juce::Label::textColourId, juce::Colour(0xff38bdf8));
    }
}

void InstrumentEditorWindow::EditorComponent::resized() {
    auto bounds = getLocalBounds().reduced(12);

    // Top Header: Instrument Selector & Sync Badge
    auto header = bounds.removeFromTop(36);
    instrumentLabel.setBounds(header.removeFromLeft(140));
    instrumentDropDown.setBounds(header.removeFromLeft(200));
    header.removeFromLeft(16);
    syncBadgeLabel.setBounds(header);

    bounds.removeFromTop(8);

    // Middle Area: Waveform Viewer & ADSR Canvas
    auto middle = bounds.removeFromTop(160);
    waveformViewer.setBounds(middle.removeFromLeft(middle.getWidth() / 2 - 4));
    middle.removeFromLeft(8);
    adsrCanvas.setBounds(middle);

    bounds.removeFromTop(12);

    // Bottom Controls: Input Gain & ADSR Sliders
    auto controls = bounds;
    auto leftGain = controls.removeFromLeft(100);
    inputGainLabel.setBounds(leftGain.removeFromTop(20));
    inputGainSlider.setBounds(leftGain);

    controls.removeFromLeft(16);
    int sliderWidth = controls.getWidth() / 4 - 6;

    auto aBox = controls.removeFromLeft(sliderWidth);
    attackLabel.setBounds(aBox.removeFromTop(16));
    attackSlider.setBounds(aBox);

    auto dBox = controls.removeFromLeft(sliderWidth);
    decayLabel.setBounds(dBox.removeFromTop(16));
    decaySlider.setBounds(dBox);

    auto sBox = controls.removeFromLeft(sliderWidth);
    sustainLabel.setBounds(sBox.removeFromTop(16));
    sustainSlider.setBounds(sBox);

    auto rBox = controls;
    releaseLabel.setBounds(rBox.removeFromTop(16));
    releaseSlider.setBounds(rBox);
}

// =============================================================================
// InstrumentEditorWindow Implementation
// =============================================================================
InstrumentEditorWindow::InstrumentEditorWindow(AudioEngine& engineToControl, PresetManager& presetTarget, int targetLayerIndex)
    : DocumentWindow("Layer " + juce::String(targetLayerIndex + 1) + " — Instrument Editor",
                     juce::Colour(0xff09090b),
                     DocumentWindow::allButtons),
      audioEngine(engineToControl), presetManager(presetTarget), layerIndex(targetLayerIndex) {

    editorComponent = std::make_unique<EditorComponent>(engineToControl, presetTarget, targetLayerIndex);
    setContentNonOwned(editorComponent.get(), true);
    setResizable(true, true);
    setResizeLimits(650, 420, 1100, 750);
    centreWithSize(750, 480);

    audioEngine.getMidiState().addChangeListener(this);
    startTimerHz(30); // 30 Hz playhead & UI animation timer
}

InstrumentEditorWindow::~InstrumentEditorWindow() {
    stopTimer();
    audioEngine.getMidiState().removeChangeListener(this);
}

void InstrumentEditorWindow::closeButtonPressed() {
    setVisible(false);
}

void InstrumentEditorWindow::changeListenerCallback(juce::ChangeBroadcaster* /*source*/) {
    juce::String lastSample = audioEngine.getMidiState().getLastSampleName();
    editorComponent->getWaveformViewer().setSampleInfo(lastSample, 0.0f);
}

void InstrumentEditorWindow::timerCallback() {
    // 30 Hz animation update for playhead
}
