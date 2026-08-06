#pragma once
#include "../Core/MidiState.h"
#include "CustomSamplerSound.h"
#include <juce_audio_utils/juce_audio_utils.h>

// =============================================================================
// ADSR parameters — set per-layer by the GUI and stored in the Preset
// All time values are in milliseconds; sustainLevel is 0.0 – 1.0
// =============================================================================
struct AdsrParams {
    float attackMs    {  10.0f };  // Time from key-on to full volume
    float decayMs     { 100.0f };  // Time from full volume down to sustain level
    float sustainLevel{   1.0f };  // Volume held while key is depressed (0.0–1.0)
    float releaseMs   { 300.0f };  // Time from key-off to silence
};

// =============================================================================
// CustomSamplerVoice — one polyphonic voice in the sampler engine
//
// Responsibilities:
//   • Renders one active voice (RAM attack -> mmap tail)
//   • Applies per-voice pitch-down interpolation and gain staging
//   • Operates ADSR state machine (Attack -> Decay -> Sustain -> Release)
// =============================================================================
class CustomSamplerVoice : public juce::SynthesiserVoice {
public:
    explicit CustomSamplerVoice(MidiState* stateToUpdate = nullptr);
    ~CustomSamplerVoice() override = default;

    void prepare(double newSampleRate);
    void setAdsrParams(const AdsrParams& params) { adsrParams = params; computeAdsrRates(); }
    void setSampleInputGain(float gainLinear) { sampleInputGain = gainLinear; }

    bool canPlaySound(juce::SynthesiserSound* sound) override;
    void startNote(int midiNoteNumber, float velocity,
                   juce::SynthesiserSound* sound, int currentPitchWheelPosition) override;
    void stopNote(float velocity, bool allowTailOff) override;
    void pitchWheelMoved(int newPitchWheelValue) override;
    void controllerMoved(int controllerNumber, int newControllerValue) override;

    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer,
                         int startSample, int numSamples) override;

    bool isVoiceActive() const override { return adsrStage != AdsrStage::Idle; }
    float getCurrentLevel() const noexcept { return adsrLevel * staticGain; }

private:
    MidiState* midiState{ nullptr };
    const CustomSamplerSound* activeSound{ nullptr };

    double currentSampleRate{ 44100.0 };
    double sourceSamplePosition{ 0.0 };
    double pitchRatio{ 1.0 };

    float staticGain{ 1.0f };
    float sampleInputGain{ 1.0f };
    float lgain{ 1.0f };
    float rgain{ 1.0f };

    AdsrParams adsrParams;
    enum class AdsrStage { Idle, Attack, Decay, Sustain, Release };
    AdsrStage adsrStage{ AdsrStage::Idle };
    float adsrLevel{ 0.0f };

    float adsrAttackInc{ 0.0f };
    float adsrDecayDec{ 0.0f };
    float adsrReleaseDec{ 0.0f };

    // Voice Stealing De-Clicker (2ms crossfade ramp)
    float lastOutputL{ 0.0f };
    float lastOutputR{ 0.0f };
    float stolenLastL{ 0.0f };
    float stolenLastR{ 0.0f };
    int declickSamplesRemaining{ 0 };
    int declickTotalSamples{ 96 };

    void computeAdsrRates();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CustomSamplerVoice)
};
