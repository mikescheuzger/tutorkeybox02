#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>

// =============================================================================
// MasterChain — Final Audio Output Channel Strip (Logic Pro Inspired)
//
// Processing Pipeline:
//   Output Buffer → Compressor → Limiter → Soft Clipper → Master Gain → Peak Meter
//
// Responsibilities:
//   1. Dynamic Range Compression (juce::dsp::Compressor)
//   2. Brickwall Limiting (juce::dsp::Limiter)
//   3. Soft Saturation Clipper (custom std::tanh curve)
//   4. Master Output Volume Gain Staging
//   5. Real-Time VU Meter Peak Detection (atomic for GUI thread safety)
// =============================================================================
class MasterChain {
public:
    MasterChain();
    ~MasterChain() = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock);
    void reset();

    /** Main Block Processor: Processes outputBuffer through Master Channel Strip. */
    void processBlock(juce::AudioBuffer<float>& buffer, int startSample, int numSamples);

    // ── Compressor Controls ───────────────────────────────────────────────────
    void setCompressorEnabled(bool enabled) { compEnabled = enabled; }
    void setCompressorThreshold(float thresholdDb);
    void setCompressorRatio(float ratio);
    void setCompressorAttack(float attackMs);
    void setCompressorRelease(float releaseMs);

    // ── Limiter Controls ──────────────────────────────────────────────────────
    void setLimiterEnabled(bool enabled) { limEnabled = enabled; }
    void setLimiterThreshold(float thresholdDb);

    // ── Clipper Controls ──────────────────────────────────────────────────────
    void setClipperEnabled(bool enabled) { clipEnabled = enabled; }
    void setClipperThreshold(float thresholdDb);
    void setClipperDrive(float driveDb);

    // ── Master Volume & VU Meter Accessors ────────────────────────────────────
    void setMasterGain(float gainLinear) { masterGain = juce::jmax(0.0f, gainLinear); }
    float getMasterGain() const { return masterGain; }

    float getPeakLevelL() const { return peakLevelL.load(); }
    float getPeakLevelR() const { return peakLevelR.load(); }

private:
    double currentSampleRate{ 44100.0 };

    // 1. Compressor
    juce::dsp::Compressor<float> compressor;
    bool compEnabled{ true };
    float compThresholdDb{ -12.0f };
    float compRatio      { 3.0f };
    float compAttackMs   { 15.0f };
    float compReleaseMs  { 100.0f };

    // 2. Limiter
    juce::dsp::Limiter<float> limiter;
    bool limEnabled{ true };
    float limThresholdDb{ -0.5f };

    // 3. Soft Saturation Clipper (tanh curve)
    bool clipEnabled{ true };
    float clipThresholdDb{ -1.0f };
    float clipDriveDb    { 0.0f };
    float clipDriveLinear{ 1.0f };

    // 4. Master Volume
    float masterGain{ 1.0f };

    // 5. Atomic Peak VU Level Detectors (Smooth 30 Hz meters)
    std::atomic<float> peakLevelL{ 0.0f };
    std::atomic<float> peakLevelR{ 0.0f };
};
