#include "NetworkServer.h"

// =============================================================================
// Constructor — Binds Thread name and engine/state references
// =============================================================================
NetworkServer::NetworkServer(AudioEngine& engineToControl, MidiState& stateToMonitor)
    : Thread("NetworkServerThread"), audioEngine(engineToControl), midiState(stateToMonitor) {}

NetworkServer::~NetworkServer() {
    stopServer();
}

// =============================================================================
// Server Control Methods
// =============================================================================
bool NetworkServer::startServer(int port) {
    if (!socket.bindToPort(port)) {
        juce::Logger::writeToLog("NetworkServer Error: Failed to bind to UDP port " + juce::String(port));
        return false;
    }

    isRunning = true;
    startThread(juce::Thread::Priority::normal);
    juce::Logger::writeToLog("NetworkServer: Listening on UDP port " + juce::String(port));
    return true;
}

void NetworkServer::stopServer() {
    isRunning = false;
    socket.shutdown();
    stopThread(1000);
}

// =============================================================================
// Main Thread Run Loop
// =============================================================================
void NetworkServer::run() {
    uint8_t buffer[1024];

    while (!threadShouldExit() && isRunning) {
        juce::String senderIP;
        int senderPort = 0;

        int bytesRead = (socket.waitUntilReady(true, 50) > 0)
                      ? socket.read(buffer, sizeof(buffer), false, senderIP, senderPort)
                      : 0;

        if (bytesRead > 0) {
            clientAddress = juce::IPAddress(senderIP);
            clientPort = senderPort;

            if (bytesRead >= 5) { // Minimum magic header check
                if (buffer[0] == 'T' && buffer[1] == 'K' && buffer[2] == 'B' && buffer[3] == 'P') {
                    uint8_t pType = buffer[4];

                    // ── Handle Incoming Control Command ───────────────────────
                    if (pType == (uint8_t)NetworkProtocol::PacketType::ControlCommand &&
                        bytesRead >= sizeof(NetworkProtocol::ControlPacket)) {
                        auto* packet = reinterpret_cast<NetworkProtocol::ControlPacket*>(buffer);
                        audioEngine.getSynth().setLayerVolume(packet->layerIndex, packet->gain);
                        audioEngine.getSynth().setLayerMute(packet->layerIndex, packet->isMuted != 0);
                    }
                    // ── Handle Set Latency Packet ─────────────────────────────
                    else if (pType == (uint8_t)NetworkProtocol::PacketType::SetLatency &&
                             bytesRead >= sizeof(NetworkProtocol::SetLatencyPacket)) {
                        auto* latPacket = reinterpret_cast<NetworkProtocol::SetLatencyPacket*>(buffer);
                        audioEngine.setBufferSize((int)latPacket->bufferSize);
                    }
                    // ── Handle Incoming MIDI Forward (Downstream or Upstream) ─
                    else if ((pType == (uint8_t)NetworkProtocol::PacketType::MidiForwardDownstream ||
                              pType == (uint8_t)NetworkProtocol::PacketType::MidiForwardUpstream) &&
                             bytesRead >= sizeof(NetworkProtocol::MidiForwardPacket)) {
                        auto* midiPkt = reinterpret_cast<NetworkProtocol::MidiForwardPacket*>(buffer);

                        juce::MidiMessage msg;
                        if (midiPkt->data2 > 0) {
                            msg = juce::MidiMessage(midiPkt->statusByte, midiPkt->data1, midiPkt->data2);
                        } else {
                            msg = juce::MidiMessage(midiPkt->statusByte, midiPkt->data1);
                        }

                        // Inject received UDP MIDI message into active audio engine queue
                        audioEngine.postExternalMidiMessage(msg);
                    }
                }
            }
        }

        // Capped 5 Hz (200 ms) Telemetry Stream
        uint32_t now = juce::Time::getMillisecondCounter();
        if (now - lastTelemetryTime >= 200) {
            sendTelemetry();
            lastTelemetryTime = now;
        }
    }
}

// =============================================================================
// Bidirectional MIDI Message Transmission
// =============================================================================
void NetworkServer::broadcastMidiMessage(const juce::MidiMessage& message, bool isUpstream) {
    if (clientAddress.toString().isEmpty() || clientPort == 0)
        return;

    NetworkProtocol::MidiForwardPacket packet{};
    packet.packetType = isUpstream ? (uint8_t)NetworkProtocol::PacketType::MidiForwardUpstream
                                   : (uint8_t)NetworkProtocol::PacketType::MidiForwardDownstream;

    packet.channel = (uint8_t)message.getChannel();
    packet.statusByte = (uint8_t)message.getRawData()[0];
    packet.data1 = (message.getRawDataSize() > 1) ? (uint8_t)message.getRawData()[1] : 0;
    packet.data2 = (message.getRawDataSize() > 2) ? (uint8_t)message.getRawData()[2] : 0;

    socket.write(clientAddress.toString(), clientPort, &packet, sizeof(NetworkProtocol::MidiForwardPacket));
}

// =============================================================================
// 5 Hz Telemetry Broadcast
// =============================================================================
void NetworkServer::sendTelemetry() {
    if (clientAddress.toString().isEmpty() || clientPort == 0)
        return;

    NetworkProtocol::TelemetryPacket packet{};
    packet.cpuUsage = audioEngine.getCpuUsage();
    packet.activeVoices = 0; // Polyphony counter

    socket.write(clientAddress.toString(), clientPort, &packet, sizeof(NetworkProtocol::TelemetryPacket));
}
