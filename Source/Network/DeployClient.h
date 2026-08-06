#pragma once
#include "../Core/PresetManager.h"
#include "NetworkProtocol.h"
#include <functional>
#include <juce_core/juce_core.h>

/**
 * DeployClient — Mac GUI Preset & Container Deployment Client
 *
 * Implements the Smart TCP Deployment Protocol (Port 7778):
 *   1. Connects to Pi daemon (kbox.local:7778 with 127.0.0.1 fallback)
 *   2. Queries Pi for container file existence
 *   3. Streams missing .bin sample containers in 64 KB chunks with UI progress updates
 *   4. Transmits full JSON Preset payload (layers, filter, ADSR, AUX, master chain, macros)
 *   5. Receives deployment success acknowledgment
 */
struct DeploymentSnapshot {
    juce::String jsonPresetText;
    struct ContainerFile {
        int layerIndex{ 0 };
        juce::File localFile;
    };
    std::vector<ContainerFile> containers;
};

class DeployClient {
public:
    DeployClient() = default;
    ~DeployClient() = default;

    /** Progress callback signature: (progress 0.0 to 1.0, status message text) */
    using ProgressCallback = std::function<void(float progress, const juce::String& statusText)>;

    /** Build thread-safe immutable snapshot on UI thread */
    static DeploymentSnapshot createSnapshot(const PresetManager& preset);

    /** Thread-safe deployment using immutable snapshot */
    static bool deploySnapshotToHardware(const DeploymentSnapshot& snapshot,
                                        const juce::String& targetHost = "kbox.local",
                                        int                 targetPort = NetworkProtocol::DEPLOY_PORT,
                                        ProgressCallback    progressCB = nullptr);

    static bool deployToHardware(const PresetManager& preset,
                                 const juce::String&  targetHost = "kbox.local",
                                 int                  targetPort = NetworkProtocol::DEPLOY_PORT,
                                 ProgressCallback     progressCB = nullptr);
};
