#include "DeployControlBar.h"

DeployControlBar::DeployControlBar(AudioEngine &engineToControl,
                                   PresetManager &presetTarget)
    : audioEngine(engineToControl), presetManager(presetTarget) {

  addAndMakeVisible(statusBadgeLabel);
  statusBadgeLabel.setFont(juce::FontOptions(12.0f, juce::Font::bold));
  updateConnectionStatus();

  // Deploy Button
  addAndMakeVisible(deployButton);
  deployButton.setColour(juce::TextButton::buttonColourId,
                         juce::Colour(0xff10b981)); // Emerald Green
  deployButton.setColour(juce::TextButton::textColourOffId,
                         juce::Colours::white);
  deployButton.onClick = [this] { startDeployment(); };

  // MIDI Console Log Button
  addAndMakeVisible(midiConsoleButton);
  midiConsoleButton.setColour(juce::TextButton::buttonColourId,
                              juce::Colour(0xff27272a));
  midiConsoleButton.setColour(juce::TextButton::textColourOffId,
                              juce::Colour(0xff38bdf8));
  midiConsoleButton.onClick = [this] {
    if (consoleWindow == nullptr) {
      consoleWindow =
          std::make_unique<MidiConsoleWindow>(audioEngine.getMidiState());
    }
    consoleWindow->setVisible(true);
    consoleWindow->toFront(true);
  };

  // Save & Load Preset Buttons
  addAndMakeVisible(savePresetButton);
  savePresetButton.setColour(juce::TextButton::buttonColourId,
                             juce::Colour(0xff3f3f46));
  savePresetButton.onClick = [this] {
    fileChooser = std::make_unique<juce::FileChooser>(
        "Save Preset", juce::File::getCurrentWorkingDirectory(), "*.json");
    fileChooser->launchAsync(juce::FileBrowserComponent::saveMode |
                                 juce::FileBrowserComponent::canSelectFiles,
                             [this](const juce::FileChooser &fc) {
                               auto result = fc.getResult();
                               if (result.existsAsFile() ||
                                   result.getParentDirectory().exists()) {
                                 presetManager.saveToFile(result);
                               }
                             });
  };

  addAndMakeVisible(loadPresetButton);
  loadPresetButton.setColour(juce::TextButton::buttonColourId,
                             juce::Colour(0xff3f3f46));
  loadPresetButton.onClick = [this] {
    fileChooser = std::make_unique<juce::FileChooser>(
        "Load Preset", juce::File::getCurrentWorkingDirectory(), "*.json");
    fileChooser->launchAsync(juce::FileBrowserComponent::openMode |
                                 juce::FileBrowserComponent::canSelectFiles,
                             [this](const juce::FileChooser &fc) {
                               auto result = fc.getResult();
                               if (result.existsAsFile()) {
                                 presetManager.loadFromFile(result);
                               }
                             });
  };

  startTimer(3000); // 3-second background connection scanner
}

DeployControlBar::~DeployControlBar() { stopTimer(); }

void DeployControlBar::timerCallback() { updateConnectionStatus(); }

void DeployControlBar::updateConnectionStatus() {
  // Background connection test socket to kbox.local:7778
  juce::StreamingSocket testSocket;
  isPiConnected =
      testSocket.connect("kbox.local", NetworkProtocol::DEPLOY_PORT, 300);

  if (isPiConnected) {
    statusBadgeLabel.setText("KEYBOX HARDWARE CONNECTED (Pi 5)",
                             juce::dontSendNotification);
    statusBadgeLabel.setColour(juce::Label::textColourId,
                               juce::Colour(0xff10b981));
  } else {
    statusBadgeLabel.setText("STANDALONE MODE (Local Mac Engine)",
                             juce::dontSendNotification);
    statusBadgeLabel.setColour(juce::Label::textColourId,
                               juce::Colour(0xff38bdf8));
  }
}

void DeployControlBar::startDeployment() {
  isDeploying = true;
  transferProgress = 0.0;
  deployStatusMessage = "Starting deployment...";
  repaint();

  // Async thread for deployment transfer
  juce::Thread::launch([this] {
    bool ok = DeployClient::deployToHardware(
        presetManager, "kbox.local", NetworkProtocol::DEPLOY_PORT,
        [this](float progress, const juce::String &msg) {
          juce::MessageManager::callAsync([this, progress, msg] {
            transferProgress = (double)progress;
            deployStatusMessage = msg;
            repaint();
          });
        });

    juce::MessageManager::callAsync([this, ok] {
      isDeploying = false;
      deployStatusMessage = ok ? "Deploy Complete!" : "Deploy Failed!";
      repaint();
    });
  });
}

void DeployControlBar::paint(juce::Graphics &g) {
  auto bounds = getLocalBounds().toFloat();
  g.setColour(juce::Colour(0xff18181b));
  g.fillRoundedRectangle(bounds, 6.0f);

  g.setColour(juce::Colour(0xff27272a));
  g.drawRoundedRectangle(bounds, 6.0f, 1.0f);

  // Draw Transfer Progress Bar when deploying
  if (isDeploying || transferProgress > 0.0) {
    auto barArea = bounds.reduced(6.0f).removeFromBottom(6.0f);
    g.setColour(juce::Colour(0xff27272a));
    g.fillRoundedRectangle(barArea, 3.0f);

    juce::Rectangle<float> fillArea(
        barArea.getX(), barArea.getY(),
        (float)(barArea.getWidth() * transferProgress), barArea.getHeight());
    g.setColour(juce::Colour(0xff10b981));
    g.fillRoundedRectangle(fillArea, 3.0f);
  }
}

void DeployControlBar::resized() {
  auto bounds = getLocalBounds().reduced(8);

  statusBadgeLabel.setBounds(bounds.removeFromLeft(280));
  bounds.removeFromLeft(12);

  deployButton.setBounds(bounds.removeFromLeft(160));
  bounds.removeFromLeft(12);

  midiConsoleButton.setBounds(bounds.removeFromLeft(150));
  bounds.removeFromLeft(12);

  loadPresetButton.setBounds(bounds.removeFromRight(110));
  bounds.removeFromRight(8);
  savePresetButton.setBounds(bounds.removeFromRight(110));
}
