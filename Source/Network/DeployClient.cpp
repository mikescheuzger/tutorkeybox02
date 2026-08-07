#include "DeployClient.h"
#include "../Synth/SamplePackager.h"

DeploymentSnapshot DeployClient::createSnapshot(const PresetManager& preset) {
    DeploymentSnapshot snap;
    juce::var jsonVar = juce::JSON::parse(preset.toJsonString());

    for (int i = 0; i < 4; ++i) {
        const auto& layer = preset.getLayerPreset(i);
        if (layer.sampleContainerPath.isNotEmpty()) {
            juce::File f(layer.sampleContainerPath);
            juce::Logger::writeToLog("DeployClient [L" + juce::String(i+1) + "] Raw path: " + layer.sampleContainerPath);
            juce::Logger::writeToLog("DeployClient [L" + juce::String(i+1) + "] existsAsFile: " + juce::String(f.existsAsFile() ? "YES" : "NO"));

            if (!f.existsAsFile()) {
                juce::File macBase = juce::File::getCurrentWorkingDirectory().getChildFile("Samples");
                juce::Logger::writeToLog("DeployClient [L" + juce::String(i+1) + "] CWD: " + juce::File::getCurrentWorkingDirectory().getFullPathName());
                if (!macBase.isDirectory()) macBase = juce::File("/Users/mikescheuzger/Desktop/TutorKeyBox02/Samples");
                juce::Logger::writeToLog("DeployClient [L" + juce::String(i+1) + "] macBase: " + macBase.getFullPathName() + " exists: " + juce::String(macBase.isDirectory() ? "YES" : "NO"));

                juce::File found = macBase.getChildFile(f.getFileName());
                juce::Logger::writeToLog("DeployClient [L" + juce::String(i+1) + "] Attempt 1: " + found.getFullPathName() + " -> " + juce::String(found.existsAsFile() ? "FOUND" : "miss"));
                if (!found.existsAsFile()) found = macBase.getChildFile("SalamanderGrandPiano-master").getChildFile(f.getFileName());
                juce::Logger::writeToLog("DeployClient [L" + juce::String(i+1) + "] Attempt 2: " + found.getFullPathName() + " -> " + juce::String(found.existsAsFile() ? "FOUND" : "miss"));
                if (!found.existsAsFile()) found = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("Containers").getChildFile(f.getFileName());
                juce::Logger::writeToLog("DeployClient [L" + juce::String(i+1) + "] Attempt 3: " + found.getFullPathName() + " -> " + juce::String(found.existsAsFile() ? "FOUND" : "miss"));
                if (!found.existsAsFile()) found = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("Containers").getChildFile(f.getFileNameWithoutExtension() + ".bin");
                juce::Logger::writeToLog("DeployClient [L" + juce::String(i+1) + "] Attempt 4: " + found.getFullPathName() + " -> " + juce::String(found.existsAsFile() ? "FOUND" : "miss"));

                if (found.existsAsFile()) f = found;
            }

            if (f.existsAsFile()) {
                juce::Logger::writeToLog("DeployClient [L" + juce::String(i+1) + "] Resolved to: " + f.getFullPathName());
                if (f.getFileExtension().equalsIgnoreCase(".sfz")) {
                    juce::File binCandidate = f.getParentDirectory().getChildFile(f.getFileNameWithoutExtension() + ".bin");
                    juce::Logger::writeToLog("DeployClient [L" + juce::String(i+1) + "] .sfz detected. binCandidate: " + binCandidate.getFullPathName() + " exists: " + juce::String(binCandidate.existsAsFile() ? "YES" : "NO"));
                    if (binCandidate.existsAsFile()) {
                        f = binCandidate;
                    } else {
                        juce::File targetBin = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                                                .getChildFile("Containers")
                                                .getChildFile(f.getFileNameWithoutExtension() + ".bin");
                        targetBin.getParentDirectory().createDirectory();

                        juce::File packInput = f.getParentDirectory().getChildFile("Samples");
                        juce::Logger::writeToLog("DeployClient [L" + juce::String(i+1) + "] packInput: " + packInput.getFullPathName() + " isDir: " + juce::String(packInput.isDirectory() ? "YES" : "NO"));
                        if (!packInput.isDirectory()) packInput = f;
                        juce::Logger::writeToLog("DeployClient [L" + juce::String(i+1) + "] targetBin: " + targetBin.getFullPathName());
                        juce::Logger::writeToLog("DeployClient [L" + juce::String(i+1) + "] Calling SamplePackager::createPackage...");

                        if (SamplePackager::createPackage(packInput, targetBin)) {
                            juce::Logger::writeToLog("DeployClient [L" + juce::String(i+1) + "] createPackage SUCCESS -> " + targetBin.getFileName());
                            f = targetBin;
                        } else {
                            juce::Logger::writeToLog("DeployClient [L" + juce::String(i+1) + "] createPackage FAILED. Falling back to raw .sfz.");
                        }
                    }
                }

                juce::Logger::writeToLog("DeployClient [L" + juce::String(i+1) + "] Final file to stream: " + f.getFullPathName());
                snap.containers.push_back({ i, f });
                if (auto* layersArray = jsonVar["layers"].getArray()) {
                    if (i < layersArray->size()) {
                        (*layersArray)[i].getDynamicObject()->setProperty("sampleContainerPath", f.getFileName());
                    }
                }
            } else {
                juce::Logger::writeToLog("DeployClient [L" + juce::String(i+1) + "] ERROR: Could not resolve file on Mac disk. Layer skipped.");
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

    updateProgress(0.05f, "Connecting to KeyBox...");

    // 1. Try USB Ethernet gadget first (10.55.0.1) - USB 3.0 speed when Pi connected via USB-C
    bool connected = false;
    juce::String activeHost = targetHost;

    {
        juce::StreamingSocket usbProbe;
        if (usbProbe.connect("10.55.0.1", targetPort, 300)) {
            juce::Logger::writeToLog("DeployClient: USB Ethernet detected at 10.55.0.1. Using USB path.");
            updateProgress(0.06f, "USB connected - deploying at full USB speed...");
            activeHost = "10.55.0.1";
        }
    }

    // 2. Connect on the chosen host (USB or Wi-Fi)
    connected = socket.connect(activeHost, targetPort, 2000);

    // 3. Fallback to Wi-Fi hostname if USB and primary failed
    if (!connected && activeHost != targetHost) {
        juce::Logger::writeToLog("DeployClient: USB path failed. Falling back to " + targetHost + "...");
        activeHost = targetHost;
        connected = socket.connect(activeHost, targetPort, 1500);
    }

    if (!connected) {
        juce::Logger::writeToLog("DeployClient Error: Could not connect to KeyBox on any path.");
        updateProgress(0.0f, "Error: Connection failed!");
        return false;
    }

    juce::Logger::writeToLog("DeployClient: Connected via " + activeHost);

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
