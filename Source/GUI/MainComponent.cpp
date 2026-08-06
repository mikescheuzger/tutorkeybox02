#include "MainComponent.h"
#include "../Synth/SampleContainerReader.h"

MainComponent::MainComponent() {
  // 1. Initialise Audio Hardware Engine
  audioEngine.initialize();

  // 2. Wire bidirectional MIDI callback to MacroEngine (Multi-listener safe)
  audioEngine.addMidiMessageListener([this](const juce::MidiMessage &msg) {
    macroEngine.processMidiMessage(msg);
  });

  // 3. Add Control Bar & Telemetry Header
  addAndMakeVisible(controlBar);
  addAndMakeVisible(telemetryView);

  // 4. Add 4 Layer Cards
  for (int i = 0; i < 4; ++i) {
    layerCards[i] =
        std::make_unique<LayerCardComponent>(audioEngine, presetManager, i);
    addAndMakeVisible(layerCards[i].get());
  }

  // 5. Add 5th FX Card (Delay first, Reverb second)
  addAndMakeVisible(fxCard);

  // 6. Add Macro Control Panel
  addAndMakeVisible(macroPanel);

  // 7. Add Master Channel Strip
  addAndMakeVisible(masterStrip);

  presetManager.addChangeListener(this);

  setSize(1360, 880);
}

MainComponent::~MainComponent() {
  presetManager.removeChangeListener(this);
}

void MainComponent::changeListenerCallback(juce::ChangeBroadcaster* source) {
  if (source == &presetManager) {
    updateAllGuiFromPreset();
  }
}

void MainComponent::updateAllGuiFromPreset() {
  for (int i = 0; i < 4; ++i) {
    if (layerCards[i] != nullptr) {
      layerCards[i]->updateFromPreset();
    }
  }
  repaint();
}

void MainComponent::paint(juce::Graphics &g) {
  g.fillAll(juce::Colour(0xff09090b)); // Premium dark background
}

void MainComponent::resized() {
  auto bounds = getLocalBounds().reduced(10);

  // Top Header Control Bar & Telemetry Display
  controlBar.setBounds(bounds.removeFromTop(44));
  bounds.removeFromTop(6);
  telemetryView.setBounds(bounds.removeFromTop(38));
  bounds.removeFromTop(10);

  // Right Side: Master Channel Strip (Logic Pro style)
  masterStrip.setBounds(bounds.removeFromRight(210));
  bounds.removeFromRight(10);

  // Bottom Area: Macro Control Panel (Allocated 270px for roomy layout)
  macroPanel.setBounds(bounds.removeFromBottom(270));
  bounds.removeFromBottom(10);

  // Center Top Area: 5 Cards (4 Layer Cards + 1 FX Card)
  int cardWidth = bounds.getWidth() / 5 - 8;

  for (int i = 0; i < 4; ++i) {
    layerCards[i]->setBounds(bounds.removeFromLeft(cardWidth));
    bounds.removeFromLeft(8);
  }

  fxCard.setBounds(bounds);
}
