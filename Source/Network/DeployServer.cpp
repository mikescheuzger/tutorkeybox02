#include "DeployServer.h"
#include "../Synth/SampleContainerReader.h"

// =============================================================================
// Constructor — Binds Thread name and engine/preset references
// =============================================================================
DeployServer::DeployServer(AudioEngine& engineToControl, PresetManager& presetTarget)
    : Thread("DeployServerThread"), audioEngine(engineToControl), presetManager(presetTarget) {}

DeployServer::~DeployServer() {
    stopServer();
}

// =============================================================================
// Server Control Methods
// =============================================================================
bool DeployServer::startServer() {
    if (!serverSocket.createListener(DEPLOY_PORT)) {
        juce::Logger::writeToLog("DeployServer Error: Failed to bind TCP listener on port " + juce::String(DEPLOY_PORT));
        return false;
    }

    isRunning = true;
    startThread(juce::Thread::Priority::normal);
    juce::Logger::writeToLog("DeployServer Success: Listening on TCP port " + juce::String(DEPLOY_PORT));
    return true;
}

void DeployServer::stopServer() {
    isRunning = false;
    signalThreadShouldExit();
    serverSocket.close();
    stopThread(2000);
}

// =============================================================================
// Thread Worker — Accepts incoming TCP streaming connections from Mac
// =============================================================================
void DeployServer::run() {
    while (!threadShouldExit() && isRunning) {
        auto* clientSocket = serverSocket.waitForNextConnection();

        if (clientSocket != nullptr && isRunning) {
            juce::Logger::writeToLog("DeployServer: Client connected from " + clientSocket->getHostName());
            handleIncomingClient(clientSocket);
        }
    }
}

// =============================================================================
// Stream Receiver — Power-cut safe chunked container file & preset payload decoder
// =============================================================================
void DeployServer::handleIncomingClient(juce::StreamingSocket* clientSocket) {
    if (clientSocket == nullptr)
        return;

    std::unique_ptr<juce::StreamingSocket> socket(clientSocket);

    NetworkProtocol::DeployHeader header{};
    if (socket->read(&header, sizeof(NetworkProtocol::DeployHeader), true) <= 0)
        return;

    // Verify protocol magic identifier "TKBD"
    if (header.magic[0] != 'T' || header.magic[1] != 'K' ||
        header.magic[2] != 'B' || header.magic[3] != 'D') {
        juce::Logger::writeToLog("DeployServer Warning: Received invalid magic header packet!");
        return;
    }

    auto opCode = (NetworkProtocol::DeployOpCode)header.opCode;

    // ── 1. Handshake Query ───────────────────────────────────────────────────
    if (opCode == NetworkProtocol::DeployOpCode::HandshakeQuery) {
        NetworkProtocol::DeployHeader respHeader{};
        respHeader.magic[0] = 'T'; respHeader.magic[1] = 'K';
        respHeader.magic[2] = 'B'; respHeader.magic[3] = 'D';
        respHeader.opCode = (uint8_t)NetworkProtocol::DeployOpCode::HandshakeResponse;
        respHeader.payloadSize = 0;

        socket->write(&respHeader, sizeof(NetworkProtocol::DeployHeader));
        return;
    }

    // ── 2. Container File Chunk Streaming (Power-cut safe write to .tmp) ─────
    if (opCode == NetworkProtocol::DeployOpCode::ContainerChunk) {
        NetworkProtocol::ContainerChunkHeader chunkHeader{};
        if (socket->read(&chunkHeader, sizeof(NetworkProtocol::ContainerChunkHeader), true) <= 0)
            return;

        juce::String targetFileName(chunkHeader.containerFileName);
        juce::File tmpFile = juce::File::getCurrentWorkingDirectory().getChildFile(targetFileName + ".tmp");

        // First chunk resets file; subsequent chunks append
        if (chunkHeader.chunkOffset == 0) {
            tmpFile.deleteFile();
        }

        juce::FileOutputStream outStream(tmpFile);
        if (!outStream.openedOk())
            return;

        outStream.setPosition((juce::int64)chunkHeader.chunkOffset);

        juce::HeapBlock<char> chunkBuffer(chunkHeader.chunkSize);
        int readNow = socket->read(chunkBuffer.getData(), (int)chunkHeader.chunkSize, true);

        if (readNow > 0) {
            outStream.write(chunkBuffer.getData(), (size_t)readNow);
            outStream.flush();
        }

        // Final chunk triggers atomic move to .bin file
        if (chunkHeader.isLastChunk != 0) {
            juce::File finalBinFile = juce::File::getCurrentWorkingDirectory().getChildFile(targetFileName);
            finalBinFile.deleteFile();
            tmpFile.moveFileTo(finalBinFile);

            juce::Logger::writeToLog("DeployServer Success: Received complete binary container -> " + finalBinFile.getFileName());

            // Mount sample container into LayeredSynth
            SampleContainerReader::loadContainerFile(finalBinFile, audioEngine.getSynth(), chunkHeader.layerIndex);
        }

        // Send Acknowledge
        NetworkProtocol::DeployHeader ackHeader{};
        ackHeader.magic[0] = 'T'; ackHeader.magic[1] = 'K';
        ackHeader.magic[2] = 'B'; ackHeader.magic[3] = 'D';
        ackHeader.opCode = (uint8_t)NetworkProtocol::DeployOpCode::DeployAck;
        socket->write(&ackHeader, sizeof(NetworkProtocol::DeployHeader));
        return;
    }

    // ── 3. Live Preset Payload Application & Persistence ────────────────────
    if (opCode == NetworkProtocol::DeployOpCode::PresetPayload) {
        uint32_t jsonLen = header.payloadSize;
        if (jsonLen == 0 || jsonLen > 1024 * 1024)
            return;

        juce::HeapBlock<char> jsonBuffer(jsonLen + 1);
        if (socket->read(jsonBuffer.getData(), (int)jsonLen, true) <= 0)
            return;
        jsonBuffer[jsonLen] = '\0';

        juce::String jsonString(jsonBuffer.getData());

        juce::File presetFile = juce::File::getCurrentWorkingDirectory().getChildFile("preset.json");
        presetFile.replaceWithText(jsonString);

        if (presetManager.loadFromFile(presetFile)) {
            juce::Logger::writeToLog("DeployServer Success: Updated and saved active preset.json!");

            // Live-update 4 Layer synths
            for (int i = 0; i < 4; ++i) {
                const auto& layer = presetManager.getLayerPreset(i);
                audioEngine.getSynth().setLayerVolume(i, layer.volume);
                audioEngine.getSynth().setLayerMute(i, layer.muted);
                audioEngine.getSynth().setLayerSampleInputGain(i, layer.sampleInputGain);
                audioEngine.getSynth().setLayerAuxSend(i, layer.auxSendGain);
                audioEngine.getSynth().setLayerFilterCutoff(i, layer.filterCutoffHz);
                audioEngine.getSynth().setLayerFilterResonance(i, layer.filterResonanceQ);
                audioEngine.getSynth().setLayerAdsr(i, { layer.attackMs, layer.decayMs,
                                                           layer.sustainLevel, layer.releaseMs });

                if (layer.sampleContainerPath.isNotEmpty()) {
                    juce::File binFile(layer.sampleContainerPath);
                    if (!juce::File::isAbsolutePath(layer.sampleContainerPath)) {
                        binFile = juce::File::getCurrentWorkingDirectory().getChildFile(layer.sampleContainerPath);
                    }
                    if (binFile.existsAsFile()) {
                        SampleContainerReader::loadContainerFile(binFile, audioEngine.getSynth(), i);
                    }
                }
            }

            // Live-update MasterChain
            const auto& mc = presetManager.getMasterChainPreset();
            auto& masterChain = audioEngine.getMasterChain();
            masterChain.setCompressorEnabled(mc.compEnabled);
            masterChain.setCompressorThreshold(mc.compThresholdDb);
            masterChain.setCompressorRatio(mc.compRatio);
            masterChain.setCompressorAttack(mc.compAttackMs);
            masterChain.setCompressorRelease(mc.compReleaseMs);

            masterChain.setLimiterEnabled(mc.limEnabled);
            masterChain.setLimiterThreshold(mc.limThresholdDb);

            masterChain.setClipperEnabled(mc.clipEnabled);
            masterChain.setClipperThreshold(mc.clipThresholdDb);
            masterChain.setClipperDrive(mc.clipDriveDb);

            masterChain.setMasterGain(mc.masterGain);
        }

        // Send Acknowledge
        NetworkProtocol::DeployHeader ackHeader{};
        ackHeader.magic[0] = 'T'; ackHeader.magic[1] = 'K';
        ackHeader.magic[2] = 'B'; ackHeader.magic[3] = 'D';
        ackHeader.opCode = (uint8_t)NetworkProtocol::DeployOpCode::DeployAck;
        socket->write(&ackHeader, sizeof(NetworkProtocol::DeployHeader));
    }
}
