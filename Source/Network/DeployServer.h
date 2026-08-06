#pragma once
#include "../Audio/AudioEngine.h"
#include "../Core/PresetManager.h"
#include "NetworkProtocol.h"
#include <juce_core/juce_core.h>

/**
 * DeployServer — Pi Daemon TCP Deployment Server (Port 7778)
 *
 * Responsibilities:
 *   • Receives 64 KB binary sample container chunks from Mac GUI
 *   • Writes streaming chunks to power-cut safe .tmp files before renaming to .bin
 *   • Loads deployed JSON presets and updates live AudioEngine (volumes, mutes, filters, ADSR, AUX, master chain)
 *   • Mounts sample containers into LayeredSynth
 *   • Persists active configuration to preset.json on local disk
 */
class DeployServer : public juce::Thread {
public:
    static constexpr int DEPLOY_PORT = NetworkProtocol::DEPLOY_PORT;

    DeployServer(AudioEngine& engineToControl, PresetManager& presetTarget);
    ~DeployServer() override;

    bool startServer();
    void stopServer();

    void run() override;

private:
    AudioEngine&   audioEngine;
    PresetManager& presetManager;

    juce::StreamingSocket serverSocket;
    bool isRunning{ false };

    void handleIncomingClient(juce::StreamingSocket* clientSocket);
};
