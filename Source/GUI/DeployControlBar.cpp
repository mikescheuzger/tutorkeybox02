#include "DeployControlBar.h"
#include "../Synth/SampleContainerReader.h"

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

  // Audio & MIDI Setup Button
  addAndMakeVisible(audioSettingsButton);
  audioSettingsButton.setColour(juce::TextButton::buttonColourId,
                                juce::Colour(0xff27272a));
  audioSettingsButton.setColour(juce::TextButton::textColourOffId,
                                juce::Colour(0xff38bdf8));
  audioSettingsButton.onClick = [this] {
    if (audioSettingsWindow == nullptr) {
      audioSettingsWindow =
          std::make_unique<AudioSettingsWindow>(audioEngine.getDeviceManager());
    }
    audioSettingsWindow->setVisible(true);
    audioSettingsWindow->toFront(true);
  };

  // MIDI Console Log Button
  addAndMakeVisible(midiConsoleButton);
  midiConsoleButton.setColour(juce::TextButton::buttonColourId,
                              juce::Colour(0xff27272a));
  midiConsoleButton.setColour(juce::TextButton::textColourOffId,
                              juce::Colour(0xff38bdf8));
  midiConsoleButton.onClick = [this] {
    if (consoleWindow == nullptr) {
      consoleWindow =
          std::make_unique<MidiConsoleWindow>(audioEngine);
    }
    consoleWindow->setVisible(true);
    consoleWindow->toFront(true);
  };

  // Wire AudioEngine live MIDI callback via multi-listener broadcast
  audioEngine.addMidiMessageListener([this](const juce::MidiMessage& msg) {
    if (consoleWindow != nullptr && consoleWindow->isVisible()) {
      consoleWindow->logMidiMessage(msg);
    }
  });

  addAndMakeVisible(presetBadgeLabel);
  presetBadgeLabel.setFont(juce::FontOptions(11.0f, juce::Font::bold));
  presetBadgeLabel.setColour(juce::Label::textColourId, juce::Colour(0xffa1a1aa));

  addAndMakeVisible(sampleLoadingStatusLabel);
  sampleLoadingStatusLabel.setFont(juce::FontOptions(11.0f, juce::Font::bold));
  sampleLoadingStatusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff10b981));
  sampleLoadingStatusLabel.setText("READY TO PLAY", juce::dontSendNotification);

  // Save & Load Preset Buttons
  addAndMakeVisible(savePresetButton);
  savePresetButton.setColour(juce::TextButton::buttonColourId,
                             juce::Colour(0xff3f3f46));
  savePresetButton.onClick = [this] {
    juce::File presetDir = juce::File::getCurrentWorkingDirectory().getChildFile("CustomPresets");
    if (!presetDir.isDirectory()) {
      presetDir = juce::File("/Users/mikescheuzger/Desktop/TutorKeyBox02/CustomPresets");
    }
    presetDir.createDirectory();

    fileChooser = std::make_unique<juce::FileChooser>(
        "Save Preset", presetDir, "*.json");
    fileChooser->launchAsync(juce::FileBrowserComponent::saveMode |
                                 juce::FileBrowserComponent::canSelectFiles,
                             [this](const juce::FileChooser &fc) {
                               auto result = fc.getResult();
                               if (result.existsAsFile() ||
                                   result.getParentDirectory().exists()) {
                                 presetManager.saveToFile(result);
                                 setPresetName(result.getFileNameWithoutExtension());
                               }
                             });
  };

  addAndMakeVisible(loadPresetButton);
  loadPresetButton.setColour(juce::TextButton::buttonColourId,
                             juce::Colour(0xff3f3f46));
  loadPresetButton.onClick = [this] {
    juce::File presetDir = juce::File::getCurrentWorkingDirectory().getChildFile("CustomPresets");
    if (!presetDir.isDirectory()) {
      presetDir = juce::File("/Users/mikescheuzger/Desktop/TutorKeyBox02/CustomPresets");
    }
    presetDir.createDirectory();

    fileChooser = std::make_unique<juce::FileChooser>(
        "Load Preset", presetDir, "*.json");
    fileChooser->launchAsync(juce::FileBrowserComponent::openMode |
                                 juce::FileBrowserComponent::canSelectFiles,
                             [this](const juce::FileChooser &fc) {
                               auto result = fc.getResult();
                               if (result.existsAsFile()) {
                                 presetManager.loadFromFile(result);
                                 setPresetName(result.getFileNameWithoutExtension());

                                 // 🎵 Apply all Layer parameters & auto-load sample files into layers 1..4
                                 for (int i = 0; i < 4; ++i) {
                                   auto lp = presetManager.getLayerPreset(i);
                                   audioEngine.getSynth().setLayerVolume(i, lp.volume);
                                   audioEngine.getSynth().setLayerMute(i, lp.muted);
                                   audioEngine.getSynth().setLayerFilterCutoff(i, lp.filterCutoffHz);
                                   audioEngine.getSynth().setLayerFilterResonance(i, lp.filterResonanceQ);
                                   audioEngine.getSynth().setLayerAdsr(i, { lp.attackMs, lp.decayMs, lp.sustainLevel, lp.releaseMs });
                                   audioEngine.getSynth().setLayerSampleInputGain(i, lp.sampleInputGain);
                                   audioEngine.getSynth().setLayerAuxSend(i, lp.auxSendGain);

                                   if (lp.sampleContainerPath.isNotEmpty()) {
                                     setSampleLoadingStatus("LOADING SAMPLES (Layer " + juce::String(i + 1) + "/4)...", false);
                                     juce::File sampleFile(lp.sampleContainerPath);
                                     if (!sampleFile.exists()) {
                                       sampleFile = juce::File::getCurrentWorkingDirectory().getChildFile("Samples").getChildFile(lp.sampleContainerPath);
                                     }
                                     if (!sampleFile.exists()) {
                                       sampleFile = juce::File("/Users/mikescheuzger/Desktop/TutorKeyBox02/Samples").getChildFile(lp.sampleContainerPath);
                                     }
                                     if (sampleFile.exists()) {
                                       SampleContainerReader::loadContainerFile(sampleFile, audioEngine.getSynth(), i);
                                       juce::Logger::writeToLog("DeployControlBar: Auto-loaded preset sample " + sampleFile.getFileName() + " into Layer " + juce::String(i + 1));
                                     }
                                   }
                                 }

                                 // Apply Master Chain parameters to AudioEngine DSP
                                 const auto& mc = presetManager.getMasterChainPreset();
                                 audioEngine.getMasterChain().setCompressorEnabled(mc.compEnabled);
                                 audioEngine.getMasterChain().setCompressorThreshold(mc.compThresholdDb);
                                 audioEngine.getMasterChain().setCompressorRatio(mc.compRatio);
                                 audioEngine.getMasterChain().setCompressorAttack(mc.compAttackMs);
                                 audioEngine.getMasterChain().setCompressorRelease(mc.compReleaseMs);

                                 audioEngine.getMasterChain().setLimiterEnabled(mc.limEnabled);
                                 audioEngine.getMasterChain().setLimiterThreshold(mc.limThresholdDb);

                                 audioEngine.getMasterChain().setClipperEnabled(mc.clipEnabled);
                                 audioEngine.getMasterChain().setClipperThreshold(mc.clipThresholdDb);
                                 audioEngine.getMasterChain().setClipperDrive(mc.clipDriveDb);

                                 audioEngine.getMasterChain().setMasterGain(mc.masterGain);

                                 setSampleLoadingStatus("READY TO PLAY", true);
                               }
                             });
  };

  startTimer(3000); // 3-second background connection scanner
}

DeployControlBar::~DeployControlBar() { stopTimer(); }

void DeployControlBar::setPresetName(const juce::String& name) {
  presetBadgeLabel.setText("PRESET: [" + name + "]", juce::dontSendNotification);
  repaint();
}

void DeployControlBar::setSampleLoadingStatus(const juce::String& text, bool isReady) {
  sampleLoadingStatusLabel.setText(text, juce::dontSendNotification);
  sampleLoadingStatusLabel.setColour(juce::Label::textColourId, isReady ? juce::Colour(0xff10b981) : juce::Colour(0xfff59e0b));
  repaint();
}

void DeployControlBar::timerCallback() { updateConnectionStatus(); }

void DeployControlBar::updateConnectionStatus() {
  // Background connection test socket to kbox.local:7778
  juce::StreamingSocket testSocket;
  isPiConnected =
      testSocket.connect("kbox.local", 7778, 1000); // 1-second timeout
  if (isPiConnected) {
    statusBadgeLabel.setText("HARDWARE SYNC: KEYBOX PI 5 ONLINE",
                             juce::dontSendNotification);
    statusBadgeLabel.setColour(juce::Label::textColourId,
                               juce::Colour(0xff10b981)); // Emerald Green

    // Send UDP registration ping to Pi 5 (Port 7777) so NetworkServer streams telemetry & MIDI back
    juce::DatagramSocket udpSocket;
    char pingBuf[5] = { 'T', 'K', 'B', 'P', 0 };
    udpSocket.write("kbox.local", 7777, pingBuf, 5);
  } else {
    statusBadgeLabel.setText("MAC STANDALONE ENGINE",
                             juce::dontSendNotification);
    statusBadgeLabel.setColour(juce::Label::textColourId,
                               juce::Colour(0xff38bdf8)); // Cyan
  }
}

void DeployControlBar::startDeployment() {
  if (isDeploying) return;
  isDeploying = true;
  transferProgress = 0.0;
  deployStatusMessage = "Starting deployment...";
  repaint();

  // Capture thread-safe snapshot on main UI thread
  auto snapshot = DeployClient::createSnapshot(presetManager);

  juce::Thread::launch([this, snapshot] {
    bool ok = false;
    try {
      ok = DeployClient::deploySnapshotToHardware(
          snapshot, "kbox.local", NetworkProtocol::DEPLOY_PORT,
          [this](float progress, const juce::String &msg) {
            juce::MessageManager::callAsync([this, progress, msg] {
              transferProgress = (double)progress;
              deployStatusMessage = msg;
              repaint();
            });
          });
    } catch (...) {
      ok = false;
    }

    juce::MessageManager::callAsync([this, ok] {
      isDeploying = false;
      deployStatusMessage = ok ? "Deploy Complete!" : "Deploy Failed!";
      repaint();
    });
  });
}

void DeployControlBar::paint(juce::Graphics &g) {
  g.fillAll(juce::Colour(0xff18181b)); // Dark Zinc Background
  auto bounds = getLocalBounds().toFloat();

  // Bottom Border Accent Line
  g.setColour(juce::Colour(0xff27272a));
  g.drawHorizontalLine(getHeight() - 1, 0.0f, bounds.getWidth());

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

  statusBadgeLabel.setBounds(bounds.removeFromLeft(220));
  bounds.removeFromLeft(6);

  deployButton.setBounds(bounds.removeFromLeft(140));
  bounds.removeFromLeft(6);

  audioSettingsButton.setBounds(bounds.removeFromLeft(140));
  bounds.removeFromLeft(6);

  midiConsoleButton.setBounds(bounds.removeFromLeft(130));
  bounds.removeFromLeft(6);

  presetBadgeLabel.setBounds(bounds.removeFromLeft(180));
  bounds.removeFromLeft(6);

  sampleLoadingStatusLabel.setBounds(bounds.removeFromLeft(150));

  loadPresetButton.setBounds(bounds.removeFromRight(100));
  bounds.removeFromRight(6);
  savePresetButton.setBounds(bounds.removeFromRight(100));
}
