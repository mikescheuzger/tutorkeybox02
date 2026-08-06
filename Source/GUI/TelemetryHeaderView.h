#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "../Audio/AudioEngine.h"

// =============================================================================
// TelemetryHeaderView — Real-Time System Telemetry Display for Mac & Pi 5
//
// Features:
//   • Dual Telemetry Readouts (Side-by-Side):
//       - Left Box: MacBook Pro local CPU load %, total RAM MB
//       - Right Box: Raspberry Pi 5 remote CPU load %, active voice count (0-128)
//   • Real-Time 10 Hz Refresh Rate via Timer
// =============================================================================
class TelemetryHeaderView : public juce::Component,
                            private juce::Timer {
public:
    explicit TelemetryHeaderView(AudioEngine& engineToMonitor);
    ~TelemetryHeaderView() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    void updateRemotePiTelemetry(float cpuUsage, uint32_t activeVoices);

private:
    AudioEngine& audioEngine;

    float        macCpuLoad{ 0.0f };
    float        piCpuLoad{ 0.0f };
    uint32_t     piActiveVoices{ 0 };
    bool         isPiConnected{ false };

    juce::Label macTitleLabel{ "MacTitle", "MACBOOK PRO TELEMETRY" };
    juce::Label macStatsLabel;

    juce::Label piTitleLabel{ "PiTitle", "KEYBOX PI 5 TELEMETRY" };
    juce::Label piStatsLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TelemetryHeaderView)
};
