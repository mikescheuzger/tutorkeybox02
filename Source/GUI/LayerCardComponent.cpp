#include "LayerCardComponent.h"

LayerCardComponent::LayerCardComponent(AudioEngine &engineToControl,
                                       PresetManager &presetTarget, int idx)
    : audioEngine(engineToControl), presetManager(presetTarget),
      layerIndex(idx) {

  titleLabel.setText("LAYER " + juce::String(layerIndex + 1),
                     juce::dontSendNotification);
  titleLabel.setFont(juce::FontOptions(12.0f, juce::Font::bold));
  titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
  addAndMakeVisible(titleLabel);

  addAndMakeVisible(instrumentBadgeButton);
  instrumentBadgeButton.setColour(juce::TextButton::buttonColourId,
                                  juce::Colour(0xff27272a));
  instrumentBadgeButton.setColour(juce::TextButton::textColourOffId,
                                  juce::Colour(0xff38bdf8));
  instrumentBadgeButton.onClick = [this] {
    if (editorWindow == nullptr) {
      editorWindow = std::make_unique<InstrumentEditorWindow>(
          audioEngine, presetManager, layerIndex);
    }
    editorWindow->setVisible(true);
    editorWindow->toFront(true);
  };

  addAndMakeVisible(clearButton);
  clearButton.setColour(juce::TextButton::buttonColourId,
                        juce::Colour(0xff3f3f46));
  clearButton.setColour(juce::TextButton::textColourOffId,
                        juce::Colours::lightgrey);
  clearButton.onClick = [this] {
    audioEngine.getSynth().clearLayer(layerIndex);
    auto layer = presetManager.getLayerPreset(layerIndex);
    layer.sampleContainerPath = "";
    presetManager.setLayerPreset(layerIndex, layer);
    updateFromPreset();
  };

  // ── Resonant Filter Knobs ─────────────────────────────────────────────────
  addAndMakeVisible(filterHeaderLabel);
  filterHeaderLabel.setFont(juce::FontOptions(10.0f, juce::Font::bold));
  filterHeaderLabel.setColour(juce::Label::textColourId, juce::Colours::grey);

  auto setupRotary = [this](juce::Slider &s, juce::Label &l) {
    addAndMakeVisible(s);
    s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 45, 14);

    addAndMakeVisible(l);
    l.setFont(juce::FontOptions(9.0f));
    l.setJustificationType(juce::Justification::centred);
  };

  setupRotary(cutoffKnob, cutoffLabel);
  cutoffKnob.setRange(20.0, 20000.0, 1.0);
  cutoffKnob.setSkewFactorFromMidPoint(1000.0); // Logarithmic frequency scale
  cutoffKnob.setValue(presetManager.getLayerPreset(layerIndex).filterCutoffHz);
  cutoffKnob.onValueChange = [this] {
    float val = (float)cutoffKnob.getValue();
    audioEngine.getSynth().setLayerFilterCutoff(layerIndex, val);
    auto layer = presetManager.getLayerPreset(layerIndex);
    layer.filterCutoffHz = val;
    presetManager.setLayerPreset(layerIndex, layer);
  };

  setupRotary(resonanceKnob, resonanceLabel);
  resonanceKnob.setRange(0.1, 10.0, 0.05);
  resonanceKnob.setValue(
      presetManager.getLayerPreset(layerIndex).filterResonanceQ);
  resonanceKnob.onValueChange = [this] {
    float val = (float)resonanceKnob.getValue();
    audioEngine.getSynth().setLayerFilterResonance(layerIndex, val);
    auto layer = presetManager.getLayerPreset(layerIndex);
    layer.filterResonanceQ = val;
    presetManager.setLayerPreset(layerIndex, layer);
  };

  // ── AUX Send Knob ────────────────────────────────────────────────────────
  setupRotary(auxSendKnob, auxSendLabel);
  auxSendKnob.setRange(0.0, 2.0, 0.01);
  auxSendKnob.setValue(presetManager.getLayerPreset(layerIndex).auxSendGain);
  auxSendKnob.onValueChange = [this] {
    float val = (float)auxSendKnob.getValue();
    audioEngine.getSynth().setLayerAuxSend(layerIndex, val);
    auto layer = presetManager.getLayerPreset(layerIndex);
    layer.auxSendGain = val;
    presetManager.setLayerPreset(layerIndex, layer);
  };

  // ── Volume & Mute Controls ───────────────────────────────────────────────
  addAndMakeVisible(volumeSlider);
  volumeSlider.setSliderStyle(juce::Slider::LinearVertical);
  volumeSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 45, 16);
  volumeSlider.setRange(0.0, 2.0, 0.01);
  volumeSlider.setValue(presetManager.getLayerPreset(layerIndex).volume);
  volumeSlider.onValueChange = [this] {
    float val = (float)volumeSlider.getValue();
    audioEngine.getSynth().setLayerVolume(layerIndex, val);
    auto layer = presetManager.getLayerPreset(layerIndex);
    layer.volume = val;
    presetManager.setLayerPreset(layerIndex, layer);
  };

  addAndMakeVisible(volumeLabel);
  volumeLabel.setFont(juce::FontOptions(9.0f));
  volumeLabel.setJustificationType(juce::Justification::centred);

  addAndMakeVisible(muteToggle);
  muteToggle.setToggleState(presetManager.getLayerPreset(layerIndex).muted,
                            juce::dontSendNotification);
  muteToggle.onClick = [this] {
    bool m = muteToggle.getToggleState();
    audioEngine.getSynth().setLayerMute(layerIndex, m);
    auto layer = presetManager.getLayerPreset(layerIndex);
    layer.muted = m;
    presetManager.setLayerPreset(layerIndex, layer);
  };

  updateFromPreset();
}

void LayerCardComponent::updateFromPreset() {
  const auto &layer = presetManager.getLayerPreset(layerIndex);
  juce::String name =
      juce::File(layer.sampleContainerPath).getFileNameWithoutExtension();
  instrumentBadgeButton.setButtonText(
      name.isNotEmpty() ? name.toUpperCase() : "NO INSTRUMENT LOADED");

  cutoffKnob.setValue(layer.filterCutoffHz, juce::dontSendNotification);
  resonanceKnob.setValue(layer.filterResonanceQ, juce::dontSendNotification);
  auxSendKnob.setValue(layer.auxSendGain, juce::dontSendNotification);
  volumeSlider.setValue(layer.volume, juce::dontSendNotification);
  muteToggle.setToggleState(layer.muted, juce::dontSendNotification);
}

void LayerCardComponent::paint(juce::Graphics &g) {
  auto bounds = getLocalBounds().toFloat().reduced(2.0f);
  g.setColour(isDragHovering ? juce::Colour(0xff0284c7)
                             : juce::Colour(0xff18181b));
  g.fillRoundedRectangle(bounds, 8.0f);

  g.setColour(isDragHovering ? juce::Colour(0xff38bdf8)
                             : juce::Colour(0xff27272a));
  g.drawRoundedRectangle(bounds, 8.0f, 1.5f);
}

void LayerCardComponent::resized() {
  auto bounds = getLocalBounds().reduced(8);

  // Top Header: Title, Badge, Clear
  auto header = bounds.removeFromTop(24);
  titleLabel.setBounds(header.removeFromLeft(60));
  clearButton.setBounds(header.removeFromRight(50));
  header.removeFromRight(6);
  instrumentBadgeButton.setBounds(header);

  bounds.removeFromTop(8);

  // Resonant Filter Section
  filterHeaderLabel.setBounds(bounds.removeFromTop(14));
  auto filterRow = bounds.removeFromTop(75);
  int knobW = filterRow.getWidth() / 3;

  auto f1 = filterRow.removeFromLeft(knobW);
  cutoffLabel.setBounds(f1.removeFromTop(12));
  cutoffKnob.setBounds(f1);

  auto f2 = filterRow.removeFromLeft(knobW);
  resonanceLabel.setBounds(f2.removeFromTop(12));
  resonanceKnob.setBounds(f2);

  auto f3 = filterRow;
  auxSendLabel.setBounds(f3.removeFromTop(12));
  auxSendKnob.setBounds(f3);

  bounds.removeFromTop(8);

  // Bottom Area: Volume Slider & Mute
  muteToggle.setBounds(bounds.removeFromBottom(22));
  bounds.removeFromBottom(4);

  volumeLabel.setBounds(bounds.removeFromTop(12));
  volumeSlider.setBounds(bounds);
}

// ── Drag & Drop Implementation ─────────────────────────────────────────────
bool LayerCardComponent::isInterestedInFileDrag(
    const juce::StringArray &files) {
  for (const auto &f : files) {
    juce::File file(f);
    if (file.isDirectory() || file.getFileExtension().equalsIgnoreCase(".sfz") ||
        file.getFileExtension().equalsIgnoreCase(".bin") || file.getFileExtension().equalsIgnoreCase(".wav") ||
        file.getFileExtension().equalsIgnoreCase(".flac"))
      return true;
  }
  return false;
}

void LayerCardComponent::filesDropped(const juce::StringArray &files, int /*x*/,
                                      int /*y*/) {
  isDragHovering = false;
  repaint();

  for (const auto &path : files) {
    juce::File f(path);
    if (f.exists()) {
      bool ok = SampleContainerReader::loadContainerFile(f, audioEngine.getSynth(),
                                               layerIndex);
      if (ok) {
        auto layer = presetManager.getLayerPreset(layerIndex);
        layer.sampleContainerPath = f.getFullPathName();
        presetManager.setLayerPreset(layerIndex, layer);
        updateFromPreset();
        break;
      }
    }
  }
}
