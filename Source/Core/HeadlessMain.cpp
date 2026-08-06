#include "../Audio/AudioEngine.h"
#include "../Synth/SampleContainerReader.h"
#include "../Core/MidiState.h"
#include "../Core/PresetManager.h"
#include "../Network/DeployServer.h"
#include "../Network/NetworkServer.h"
#include <juce_core/juce_core.h>

int main(int argc, char* argv[]) {
    juce::ScopedJuceInitialiser_GUI juceInit;

    juce::Logger::writeToLog("==========================================");
    juce::Logger::writeToLog("  TutorKeyBox 02 Core (Raspberry Pi Daemon)");
    juce::Logger::writeToLog("==========================================");

    MidiState midiState;
    AudioEngine audioEngine(midiState);
    PresetManager presetManager;

    if (!audioEngine.initialize()) {
        juce::Logger::writeToLog("Fatal Error: Failed to initialize AudioEngine on Pi!");
        return 1;
    }

    // ── 1. Boot-time Local Preset.json Loading ─────────────────────────────────
    juce::File presetFile = juce::File::getCurrentWorkingDirectory().getChildFile("preset.json");
    if (presetFile.existsAsFile()) {
        juce::Logger::writeToLog("HeadlessCore: Loading local preset.json configuration...");
        if (presetManager.loadFromFile(presetFile)) {
            // Apply loaded preset to AudioEngine layers
            for (int layerIdx = 0; layerIdx < 4; ++layerIdx) {
                const auto& layer = presetManager.getLayerPreset(layerIdx);
                audioEngine.getSynth().setLayerVolume(layerIdx, layer.volume);
                audioEngine.getSynth().setLayerMute(layerIdx, layer.muted);
                audioEngine.getSynth().setLayerSampleInputGain(layerIdx, layer.sampleInputGain);
                audioEngine.getSynth().setLayerAuxSend(layerIdx, layer.auxSendGain);
                audioEngine.getSynth().setLayerFilterCutoff(layerIdx, layer.filterCutoffHz);
                audioEngine.getSynth().setLayerFilterResonance(layerIdx, layer.filterResonanceQ);
                audioEngine.getSynth().setLayerAdsr(layerIdx, { layer.attackMs, layer.decayMs,
                                                                 layer.sustainLevel, layer.releaseMs });

                if (layer.sampleContainerPath.isNotEmpty()) {
                    juce::File binFile(layer.sampleContainerPath);
                    if (!binFile.exists()) {
                        juce::String fileName = binFile.getFileName();
                        binFile = juce::File::getCurrentWorkingDirectory().getChildFile(fileName);
                        if (!binFile.exists()) {
                            binFile = juce::File::getCurrentWorkingDirectory().getChildFile("Samples").getChildFile(fileName);
                        }
                        if (!binFile.exists()) {
                            binFile = juce::File::getSpecialLocation(juce::File::userHomeDirectory).getChildFile(".config/tutorkeybox/Containers").getChildFile(fileName);
                        }
                    }
                    if (binFile.exists()) {
                        SampleContainerReader::loadContainerFile(binFile, audioEngine.getSynth(), layerIdx);
                        juce::Logger::writeToLog("HeadlessCore: Loaded layer " + juce::String(layerIdx + 1) + " sample -> " + binFile.getFullPathName());
                    }
                }
            }
        }
    } else {
        juce::Logger::writeToLog("HeadlessCore Notice: No local preset.json found. Initializing with default parameters.");
    }

    // ── 2. Auto-load single container file if present in working directory ────
    juce::File currentDir = juce::File::getCurrentWorkingDirectory();
    auto binFiles = currentDir.findChildFiles(juce::File::findFiles, false, "*.bin");
    if (!binFiles.isEmpty()) {
        juce::Logger::writeToLog("HeadlessCore: Mounting default sample container -> " + binFiles[0].getFileName());
        SampleContainerReader::loadContainerFile(binFiles[0], audioEngine.getSynth(), 0);
    }

    // ── 3. Launch Network Server Services ──────────────────────────────────────
    NetworkServer telemetryServer(audioEngine, midiState);
    if (telemetryServer.startServer(NetworkProtocol::DEFAULT_PORT)) {
        juce::Logger::writeToLog("HeadlessCore: UDP Telemetry & MIDI Server started on port " +
                                 juce::String(NetworkProtocol::DEFAULT_PORT));
    }

    DeployServer deployServer(audioEngine, presetManager);
    if (deployServer.startServer()) {
        juce::Logger::writeToLog("HeadlessCore: TCP Smart Deployment Server started on port " +
                                 juce::String(NetworkProtocol::DEPLOY_PORT));
    }

    // Forward physical Pi MIDI controller events upstream over UDP to Mac GUI app
    audioEngine.addMidiMessageListener([&telemetryServer](const juce::MidiMessage& msg) {
        telemetryServer.broadcastMidiMessage(msg, true); // true = upstream
    });

    juce::Logger::writeToLog("HeadlessCore Status: KeyBox Daemon is live and active. Press Ctrl+C to quit.");

    // Signal handler loop for headless execution
    while (true) {
        juce::Thread::sleep(1000);
    }

    deployServer.stopServer();
    telemetryServer.stopServer();
    audioEngine.shutdown();
    return 0;
}
