#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>

// =============================================================================
// FXChannel — Master FX Card & AUX Return Audio Processor
//
// Responsibilities:
//   • Receives summed AUX send signals from all 4 synth layers
//   • Processes stereo Schroeder Reverb (juce::dsp::Reverb)
//   • Processes stereo Delay with feedback loop (juce::dsp::DelayLine)
//   • Zero heap allocation on the audio thread
//   • Exposes parameters for GUI controls (Reverb Room Size, Wet/Dry, Delay Time, Feedback)
// =============================================================================
class FXChannel {
public:
    FXChannel();
    ~FXChannel() = default;

    // ── Audio Lifecycle ───────────────────────────────────────────────────────
    void prepareToPlay(double sampleRate, int samplesPerBlock);
    void reset();

    /** Main FX Block Processor: Processes auxBuffer and sums wet FX into outputBuffer. */
    void processBlock(juce::AudioBuffer<float>& auxBuffer,
                      juce::AudioBuffer<float>& outputBuffer,
                      int startSample, int numSamples);

    // ── Reverb Controls ───────────────────────────────────────────────────────
    void setReverbEnabled(bool enabled) { reverbEnabled = enabled; }
    void setReverbRoomSize(float size0to1);
    void setReverbDamping(float damping0to1);
    void setReverbWetLevel(float wet0to1);
    void setReverbDryLevel(float dry0to1);

    // ── Delay Controls ────────────────────────────────────────────────────────
    void setDelayEnabled(bool enabled) { delayEnabled = enabled; }
    void setDelayTimeMs(float delayMs);
    void setDelayFeedback(float feedback0to1);
    void setDelayWetLevel(float wet0to1);

    // ── Master FX Output Control ──────────────────────────────────────────────
    void setFxOutputGain(float gainLinear) { fxOutputGain = juce::jmax(0.0f, gainLinear); }

    // ── Getters for UI Sync ───────────────────────────────────────────────────
    bool isDelayEnabled() const { return delayEnabled; }
    float getDelayTimeMs() const { return targetDelayTimeMs; }
    float getDelayFeedback() const { return delayFeedback; }
    float getDelayWetLevel() const { return delayWetLevel; }

    bool isReverbEnabled() const { return reverbEnabled; }
    float getReverbRoomSize() const { return reverbParams.roomSize; }
    float getReverbDamping() const { return reverbParams.damping; }
    float getReverbWetLevel() const { return reverbParams.wetLevel; }
    float getFxOutputGain() const { return fxOutputGain; }

private:
    double currentSampleRate{ 44100.0 };

    // 1. Stereo Reverb DSP Processor
    juce::dsp::Reverb reverb;
    juce::dsp::Reverb::Parameters reverbParams;
    bool reverbEnabled{ true };

    // 2. Stereo Delay DSP Processor with Feedback
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLineL{ 192000 };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLineR{ 192000 };

    float targetDelayTimeMs{ 375.0f };
    float delayFeedback    { 0.35f };
    float delayWetLevel    { 0.40f };
    bool  delayEnabled     { true };

    float fxOutputGain{ 1.0f };

    // Pre-allocated temporary delay processing buffer (Zero Heap Allocation)
    juce::AudioBuffer<float> delayBuffer;
};
