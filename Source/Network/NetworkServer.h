#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "../Audio/AudioEngine.h"
#include "NetworkProtocol.h"

/**
 * NetworkServer — UDP Telemetry & Bidirectional MIDI Forwarding Service
 *
 * Responsibilities:
 *   • Runs on UDP port 7777
 *   • Sends 5 Hz throttled telemetry (CPU usage, voice count, VU levels) to Mac GUI
 *   • Forward MIDI bidirectional:
 *       - Mac -> Pi (Downstream MIDI forward)
 *       - Pi -> Mac (Upstream MIDI forward for Pi-connected controllers)
 */
class NetworkServer : public juce::Thread {
public:
    NetworkServer(AudioEngine& engineToControl, MidiState& stateToMonitor);
    ~NetworkServer() override;

    bool startServer(int port = NetworkProtocol::DEFAULT_PORT);
    void stopServer();

    /** Broadcast local/incoming MIDI message over UDP to remote endpoint. */
    void broadcastMidiMessage(const juce::MidiMessage& message, bool isUpstream = false);

private:
    AudioEngine& audioEngine;
    MidiState&   midiState;

    juce::DatagramSocket socket{ true };
    int serverPort{ NetworkProtocol::DEFAULT_PORT };

    juce::IPAddress clientAddress;
    int clientPort{ 0 };

    bool isRunning{ false };
    juce::int64 lastTelemetryTime{ 0 };

    void run() override;
    void processIncomingPacket(const void* data, int size, const juce::IPAddress& sender, int senderPort);
    void sendTelemetry();
};
