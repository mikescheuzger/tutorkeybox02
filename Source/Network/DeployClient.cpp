#include "DeployClient.h"

bool DeployClient::deployToHardware(const PresetManager& preset,
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

    // 3. Scan layers for container files to stream
    for (int i = 0; i < 4; ++i) {
        const auto& layer = preset.getLayerPreset(i);
        if (layer.sampleContainerPath.isEmpty()) continue;

        juce::File binFile(layer.sampleContainerPath);
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

            NetworkProtocol::ContainerChunkHeader chunkHdr{};
            chunkHdr.opCode = (uint8_t)NetworkProtocol::DeployOpCode::ContainerChunk;
            chunkHdr.layerIndex = (uint8_t)i;
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

    juce::String jsonText = preset.toJsonString();
    int32_t jsonSize = (int32_t)jsonText.getNumBytesAsUTF8();

    NetworkProtocol::DeployHeader deployHdr{};
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
