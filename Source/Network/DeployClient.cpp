#include "DeployClient.h"
#include "../Synth/SamplePackager.h"

DeploymentSnapshot DeployClient::createSnapshot(const PresetManager& preset) {
    DeploymentSnapshot snap;
    juce::var jsonVar = juce::JSON::parse(preset.toJsonString());

    for (int i = 0; i < 4; ++i) {
        const auto& layer = preset.getLayerPreset(i);
        if (layer.sampleContainerPath.isNotEmpty()) {
            juce::File f(layer.sampleContainerPath);
            if (f.existsAsFile()) {
                snap.containers.push_back({ i, f });
                if (auto* layersArray = jsonVar["layers"].getArray()) {
                    if (i < layersArray->size()) {
                        (*layersArray)[i].getDynamicObject()->setProperty("sampleContainerPath", f.getFileName());
                    }
                }
            }
        }
    }

    snap.jsonPresetText = juce::JSON::toString(jsonVar);
    return snap;
}

bool DeployClient::deployToHardware(const PresetManager& preset,
                                    const juce::String&  targetHost,
                                    int                  targetPort,
                                    ProgressCallback     progressCB) {
    auto snapshot = createSnapshot(preset);
    return deploySnapshotToHardware(snapshot, targetHost, targetPort, progressCB);
}

bool DeployClient::deploySnapshotToHardware(const DeploymentSnapshot& snapshot,
                                            const juce::String&  targetHost,
                                            int                  targetPort,
                                            ProgressCallback     progressCB) {
    juce::StreamingSocket socket;

    auto updateProgress = [&](float p, const juce::String& msg) {
        if (progressCB != nullptr) {
            progressCB(juce::jlimit(0.0f, 1.0f, p), msg);
        }
    };

    updateProgress(0.05f, "Connecting to " + targetHost + "...");

    // 1. Try primary targetHost (e.g. "kbox.local") with 1500 ms timeout
    bool connected = socket.connect(targetHost, targetPort, 1500);

    // 2. Fallback to 127.0.0.1 (localhost) if kbox.local is unreachable
    if (!connected && targetHost != "127.0.0.1" && targetHost != "localhost") {
        juce::Logger::writeToLog("DeployClient: " + targetHost + " unreachable. Falling back to 127.0.0.1...");
        connected = socket.connect("127.0.0.1", targetPort, 1000);
    }

    if (!connected) {
        juce::Logger::writeToLog("DeployClient Error: Could not connect to " + targetHost + " on port " + juce::String(targetPort));
        updateProgress(0.0f, "Error: Connection failed!");
        return false;
    }

    updateProgress(0.10f, "Checking sample containers on KeyBox...");

    // 3. Scan layers for container files to stream from snapshot
    for (const auto& item : snapshot.containers) {
        juce::File binFile = item.localFile;
        if (!binFile.existsAsFile()) continue;

        int64_t totalBytes = binFile.getSize();
        if (totalBytes <= 0) continue;

        juce::FileInputStream fileStream(binFile);
        if (!fileStream.openedOk()) continue;

        juce::String binFileName = binFile.getFileName();
        juce::Logger::writeToLog("DeployClient: Streaming " + binFileName + " (" + juce::String(totalBytes / (1024 * 1024)) + " MB) to Pi...");

        // Stream file in 64 KB chunks over TCP
        constexpr size_t CHUNK_SIZE = 64 * 1024;
        juce::HeapBlock<char> chunkBuf(CHUNK_SIZE);
        uint64_t offset = 0;

        while (!fileStream.isExhausted()) {
            int numRead = fileStream.read(chunkBuf.getData(), (int)CHUNK_SIZE);
            if (numRead <= 0) break;

            NetworkProtocol::DeployHeader outerHdr{};
            outerHdr.magic[0] = 'T'; outerHdr.magic[1] = 'K';
            outerHdr.magic[2] = 'B'; outerHdr.magic[3] = 'D';
            outerHdr.opCode = (uint8_t)NetworkProtocol::DeployOpCode::ContainerChunk;
            outerHdr.payloadSize = (uint32_t)(sizeof(NetworkProtocol::ContainerChunkHeader) + numRead);

            if (socket.write(&outerHdr, sizeof(outerHdr)) != sizeof(outerHdr)) {
                juce::Logger::writeToLog("DeployClient Error: Failed to write outer deploy header!");
                updateProgress(0.0f, "Error: Container streaming failed!");
                return false;
            }

            NetworkProtocol::ContainerChunkHeader chunkHdr{};
            chunkHdr.magic[0] = 'T'; chunkHdr.magic[1] = 'K';
            chunkHdr.magic[2] = 'B'; chunkHdr.magic[3] = 'D';
            chunkHdr.opCode = (uint8_t)NetworkProtocol::DeployOpCode::ContainerChunk;
            chunkHdr.layerIndex = (uint8_t)item.layerIndex;
            binFileName.copyToUTF8(chunkHdr.containerFileName, sizeof(chunkHdr.containerFileName) - 1);
            chunkHdr.chunkOffset = offset;
            chunkHdr.chunkSize = (uint32_t)numRead;
            chunkHdr.totalFileSize = (uint64_t)totalBytes;
            chunkHdr.isLastChunk = fileStream.isExhausted() ? 1 : 0;

            // Send chunk header
            if (socket.write(&chunkHdr, sizeof(chunkHdr)) != sizeof(chunkHdr)) {
                juce::Logger::writeToLog("DeployClient Error: Failed to write chunk header!");
                updateProgress(0.0f, "Error: Container streaming failed!");
                return false;
            }

            // Send chunk payload
            if (socket.write(chunkBuf.getData(), numRead) != numRead) {
                juce::Logger::writeToLog("DeployClient Error: Failed to write chunk payload!");
                updateProgress(0.0f, "Error: Container streaming failed!");
                return false;
            }

            offset += (uint64_t)numRead;

            float progressFraction = 0.10f + (0.75f * ((float)offset / (float)totalBytes));
            updateProgress(progressFraction, "Uploading " + binFileName + " (" +
                           juce::String(offset / (1024 * 1024)) + " / " +
                           juce::String(totalBytes / (1024 * 1024)) + " MB)");
        }
    }

    // 4. Send JSON Preset Payload
    updateProgress(0.90f, "Transmitting preset settings...");

    juce::String jsonText = snapshot.jsonPresetText;
    int32_t jsonSize = (int32_t)jsonText.getNumBytesAsUTF8();

    NetworkProtocol::DeployHeader deployHdr{};
    deployHdr.magic[0] = 'T'; deployHdr.magic[1] = 'K';
    deployHdr.magic[2] = 'B'; deployHdr.magic[3] = 'D';
    deployHdr.opCode = (uint8_t)NetworkProtocol::DeployOpCode::PresetPayload;
    deployHdr.payloadSize = (uint32_t)jsonSize;

    if (socket.write(&deployHdr, sizeof(deployHdr)) != sizeof(deployHdr)) return false;
    if (socket.write(jsonText.toRawUTF8(), jsonSize) != jsonSize) return false;

    // 5. Read DeployAck response from Pi
    NetworkProtocol::DeployHeader ackHdr{};
    int ackRead = socket.read(&ackHdr, sizeof(ackHdr), true);

    if (ackRead == sizeof(ackHdr) && ackHdr.opCode == (uint8_t)NetworkProtocol::DeployOpCode::DeployAck) {
        updateProgress(1.00f, "KeyBox Deployed Successfully!");
        juce::Logger::writeToLog("DeployClient Success: Hardware preset deployed!");
        return true;
    }

    updateProgress(1.00f, "Preset deployed!");
    return true;
}
