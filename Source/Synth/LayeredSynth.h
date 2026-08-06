#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "../Core/MidiState.h"
#include "CustomSamplerSound.h"
#include "CustomSamplerVoice.h"
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>

// =============================================================================
// Helper subclass of juce::Synthesiser to expose startVoice for custom triggering
// =============================================================================
class LayerSynthesiser : public juce::Synthesiser {
public:
    void triggerVoice(juce::SynthesiserVoice* voice,
                      juce::SynthesiserSound* sound, int midiChannel,
                      int midiNoteNumber, float velocity) {
        startVoice(voice, sound, midiChannel, midiNoteNumber, velocity);
    }
};

// =============================================================================
// LayeredSynth — 4-Layer Polyphonic Synthesiser Core
//
// Features:
//   • 4 Independent Instrument Layers (32 voices per layer = 128 total polyphony)
//   • O(1) Instant Note-On Sound Lookup Grid with Round-Robin & Pedal-State support
//   • Real-Time Sustain Pedal (CC64) tracking for NoPedal / WithPedal sample selection
//   • Dedicated Release Trigger and Pedal Noise sample registries
//   • Per-Layer Resonant Low-Pass Filter (juce::dsp::StateVariableTPTFilter)
//   • Per-Layer AUX Send output bus for routing to external FX Channel
//   • Zero-Heap Allocation rendering on the real-time audio thread
// =============================================================================
class LayeredSynth {
public:
    using SoundPtr = juce::SynthesiserSound::Ptr;

    explicit LayeredSynth(MidiState& stateToUpdate);
    ~LayeredSynth() = default;

    // ── Audio Lifecycle ───────────────────────────────────────────────────────
    /** Call once before audio processing begins. Sets up sample rates & pre-allocates buffers. */
    void prepareToPlay(double sampleRate, int samplesPerBlock);

    /** Main audio render callback. Renders layers, applies filters, routes to output & aux buses. */
    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer,
                         juce::MidiBuffer& midiMessages,
                         int startSample, int numSamples,
                         juce::AudioBuffer<float>* auxOutputBuffer = nullptr);

    /** Returns total active playing polyphonic voices across all 4 layers (0 to 128). */
    int getNumActiveVoices() const;

    // ── Sound Registry Management ──────────────────────────────────────────────
    /** Route NoteOn sample to O(1) lookup grid for the layer. */
    void addNoteOnSoundToLayer(int layerIndex, SoundPtr sound);

    /** Route ReleaseTrigger sample to the layer's release registry. */
    void addReleaseSoundToLayer(int layerIndex, SoundPtr sound);

    /** Route PedalDown / PedalUp noise sample to the layer's pedal noise registry. */
    void addPedalSoundToLayer(int layerIndex, SoundPtr sound);

    /** Backward-compatible helper method. Reads sampleType and routes to proper registry. */
    void addSoundToLayer(int layerIndex, SoundPtr sound);

    /** Clear all sounds and reset lookup grids for a specific layer. */
    void clearLayer(int layerIndex);

    /** Clear all sounds across all 4 layers. */
    void clearAllLayers();

    // ── Layer Parameters (GUI & Preset Controls) ──────────────────────────────
    void setLayerVolume(int layerIndex, float gainLinear);
    void setLayerMute(int layerIndex, bool isMuted);

    void setLayerSampleInputGain(int layerIndex, float gainLinear);
    void setLayerAdsr(int layerIndex, const AdsrParams& params);
    void setLayerFilterCutoff(int layerIndex, float cutoffHz);
    void setLayerFilterResonance(int layerIndex, float resonanceQ);
    void setLayerAuxSend(int layerIndex, float auxGainLinear);

    static constexpr int NUM_LAYERS = 4;
    static constexpr int VOICES_PER_LAYER = 32;

private:
    MidiState& midiState;

    // ── Per-Layer Internal Data Structure ────────────────────────────────────
    struct Layer {
        LayerSynthesiser synth;

        // Mix parameters
        float volumeGain      { 1.0f };
        float sampleInputGain { 1.0f };
        float auxSendGain     { 0.0f };
        bool  muted           { false };

        // ADSR & Filter parameters
        AdsrParams adsrParams;
        float filterCutoffHz  { 20000.0f };
        float filterResonanceQ{ 0.707f };
        juce::dsp::StateVariableTPTFilter<float> filter;

        // Zero-heap pre-allocated audio buffer
        juce::AudioBuffer<float> tempBuffer;

        // Sound Registries
        // 1. NoteOn Grid: [Note][Velocity] -> list of variants (RR & PedalState)
        std::array<std::array<std::vector<SoundPtr>, 128>, 128> soundLookupGrid;

        // 2. Release Trigger Registry: [Note] -> list of release trigger sounds
        std::array<std::vector<SoundPtr>, 128> releaseSounds;

        // 3. Pedal Noise Registry: PedalDown and PedalUp sounds
        std::vector<SoundPtr> pedalDownSounds;
        std::vector<SoundPtr> pedalUpSounds;

        // Per-note round robin tracking counters
        std::array<uint8_t, 128> roundRobinCounters{ 0 };
    };

    std::array<Layer, NUM_LAYERS> layers;

    // Global Sustain Pedal state tracking (CC64)
    bool isPedalDown{ false };

    // Helper method to pick best matching sound variant from candidate list
    SoundPtr selectBestVariant(const std::vector<SoundPtr>& candidates,
                               PedalState currentPedalState,
                               uint8_t noteNumber, uint8_t& rrCounterRef);
};
