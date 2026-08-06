#include "TelemetryHeaderView.h"

TelemetryHeaderView::TelemetryHeaderView(AudioEngine &engineToMonitor, std::function<bool()> piConnectedChecker)
    : audioEngine(engineToMonitor), isPiConnectedChecker(piConnectedChecker) {

  addAndMakeVisible(macTitleLabel);
  macTitleLabel.setFont(juce::FontOptions(10.0f, juce::Font::bold));
  macTitleLabel.setColour(juce::Label::textColourId,
                          juce::Colour(0xff38bdf8)); // Cyan

  addAndMakeVisible(macStatsLabel);
  macStatsLabel.setFont(juce::FontOptions(11.0f, juce::Font::plain));
  macStatsLabel.setColour(juce::Label::textColourId, juce::Colours::white);

  addAndMakeVisible(piTitleLabel);
  piTitleLabel.setFont(juce::FontOptions(10.0f, juce::Font::bold));
  piTitleLabel.setColour(juce::Label::textColourId,
                         juce::Colour(0xff10b981)); // Green

  addAndMakeVisible(piStatsLabel);
  piStatsLabel.setFont(juce::FontOptions(11.0f, juce::Font::plain));
  piStatsLabel.setColour(juce::Label::textColourId, juce::Colours::white);

  startTimerHz(10); // 10 Hz telemetry refresh rate
}

TelemetryHeaderView::~TelemetryHeaderView() { stopTimer(); }

void TelemetryHeaderView::updateRemotePiTelemetry(float cpuUsage,
                                                  uint32_t activeVoices) {
  piCpuLoad = cpuUsage;
  piActiveVoices = activeVoices;
  isPiConnected = true;
}

void TelemetryHeaderView::timerCallback() {
  // 1. Gather Mac local telemetry
  macCpuLoad = audioEngine.getCpuUsage() * 100.0f;
  int ramMB = juce::SystemStats::getMemorySizeInMegabytes();

  macStatsLabel.setText("CPU: " + juce::String(macCpuLoad, 1) +
                            "%  |  RAM: " + juce::String(ramMB) +
                            " MB Total  |  Status: Nominal",
                        juce::dontSendNotification);

  // 2. Format Pi remote telemetry
  bool connected = isPiConnected || (isPiConnectedChecker != nullptr && isPiConnectedChecker());
  if (connected) {
    piStatsLabel.setText("CPU: " + juce::String(piCpuLoad * 100.0f, 1) +
                             "%  |  Voices: " + juce::String(piActiveVoices) +
                             " / 128  |  Status: Synced",
                         juce::dontSendNotification);
  } else {
    piStatsLabel.setText("Status: Standalone Mode (Searching KeyBox Pi 5...)",
                         juce::dontSendNotification);
  }

  repaint();
}

void TelemetryHeaderView::paint(juce::Graphics &g) {
  auto bounds = getLocalBounds().toFloat().reduced(2.0f);
  g.setColour(juce::Colour(0xff18181b));
  g.fillRoundedRectangle(bounds, 6.0f);

  g.setColour(juce::Colour(0xff27272a));
  g.drawRoundedRectangle(bounds, 6.0f, 1.0f);

  // Mid divider line
  g.setColour(juce::Colour(0xff3f3f46));
  g.drawVerticalLine((int)bounds.getCentreX(), bounds.getY() + 4.0f,
                     bounds.getBottom() - 4.0f);
}

void TelemetryHeaderView::resized() {
  auto bounds = getLocalBounds().reduced(8);
  auto halfWidth = bounds.getWidth() / 2 - 8;

  // Mac Side (Left)
  auto leftBox = bounds.removeFromLeft(halfWidth);
  macTitleLabel.setBounds(leftBox.removeFromTop(14));
  macStatsLabel.setBounds(leftBox);

  bounds.removeFromLeft(16);

  // Pi Side (Right)
  auto rightBox = bounds;
  piTitleLabel.setBounds(rightBox.removeFromTop(14));
  piStatsLabel.setBounds(rightBox);
}
