#include "MasterChain.h"
#include <cmath>

// =============================================================================
// Constructor — Initialises compressor and limiter defaults
// =============================================================================
MasterChain::MasterChain() {
  compressor.setThreshold(compThresholdDb);
  compressor.setRatio(compRatio);
  compressor.setAttack(compAttackMs);
  compressor.setRelease(compReleaseMs);

  limiter.setThreshold(limThresholdDb);
  limiter.setRelease(50.0f);
}

// =============================================================================
// prepareToPlay — Configures sample rate and block size for DSP modules
// =============================================================================
void MasterChain::prepareToPlay(double sampleRate, int samplesPerBlock) {
  currentSampleRate = sampleRate;
  int safeBlockSize = juce::jmax(512, samplesPerBlock);

  juce::dsp::ProcessSpec spec;
  spec.sampleRate = sampleRate;
  spec.maximumBlockSize = (juce::uint32)safeBlockSize;
  spec.numChannels = 2;

  compressor.prepare(spec);
  compressor.reset();

  limiter.prepare(spec);
  limiter.reset();

  peakLevelL.store(0.0f);
  peakLevelR.store(0.0f);
}

void MasterChain::reset() {
  compressor.reset();
  limiter.reset();
  peakLevelL.store(0.0f);
  peakLevelR.store(0.0f);
}

// =============================================================================
// Compressor Parameter Setters
// =============================================================================
void MasterChain::setCompressorThreshold(float thresholdDb) {
  compThresholdDb = juce::jlimit(-60.0f, 0.0f, thresholdDb);
  compressor.setThreshold(compThresholdDb);
}

void MasterChain::setCompressorRatio(float ratio) {
  compRatio = juce::jlimit(1.0f, 20.0f, ratio);
  compressor.setRatio(compRatio);
}

void MasterChain::setCompressorAttack(float attackMs) {
  compAttackMs = juce::jlimit(0.1f, 200.0f, attackMs);
  compressor.setAttack(compAttackMs);
}

void MasterChain::setCompressorRelease(float releaseMs) {
  compReleaseMs = juce::jlimit(10.0f, 1000.0f, releaseMs);
  compressor.setRelease(compReleaseMs);
}

// =============================================================================
// Limiter Parameter Setters
// =============================================================================
void MasterChain::setLimiterThreshold(float thresholdDb) {
  limThresholdDb = juce::jlimit(-24.0f, 0.0f, thresholdDb);
  limiter.setThreshold(limThresholdDb);
}

// =============================================================================
// Clipper Parameter Setters
// =============================================================================
void MasterChain::setClipperThreshold(float thresholdDb) {
  clipThresholdDb = juce::jlimit(-12.0f, 0.0f, thresholdDb);
}

void MasterChain::setClipperDrive(float driveDb) {
  clipDriveDb = juce::jlimit(0.0f, 24.0f, driveDb);
  clipDriveLinear = juce::Decibels::decibelsToGain(clipDriveDb);
}

// =============================================================================
// processBlock — Main Audio Thread Output Channel Strip Processing
// =============================================================================
void MasterChain::processBlock(juce::AudioBuffer<float> &buffer,
                               int startSample, int numSamples) {
  if (numSamples <= 0)
    return;

  const int numChannels = buffer.getNumChannels();

  // ── 1. Dynamic Range Compressor ───────────────────────────────────────────
  if (compEnabled) {
    juce::dsp::AudioBlock<float> block(buffer.getArrayOfWritePointers(),
                                       (size_t)numChannels, (size_t)startSample,
                                       (size_t)numSamples);
    juce::dsp::ProcessContextReplacing<float> context(block);
    compressor.process(context);
  }

  // ── 2. Brickwall Limiter ──────────────────────────────────────────────────
  if (limEnabled) {
    juce::dsp::AudioBlock<float> block(buffer.getArrayOfWritePointers(),
                                       (size_t)numChannels, (size_t)startSample,
                                       (size_t)numSamples);
    juce::dsp::ProcessContextReplacing<float> context(block);
    limiter.process(context);
  }

  // ── 3. Soft Saturation Clipper (tanh curve) ──────────────────────────────
  if (clipEnabled) {
    float clipThresholdGain = juce::Decibels::decibelsToGain(clipThresholdDb);

    for (int ch = 0; ch < numChannels; ++ch) {
      float *channelData = buffer.getWritePointer(ch, startSample);

      for (int i = 0; i < numSamples; ++i) {
        float sample = channelData[i] * clipDriveLinear;

        // Apply soft tanh clipping above threshold
        if (std::abs(sample) > clipThresholdGain) {
          float sign = (sample > 0.0f) ? 1.0f : -1.0f;
          float over = (std::abs(sample) - clipThresholdGain);
          sample = sign * (clipThresholdGain +
                           (1.0f - clipThresholdGain) * std::tanh(over));
        }

        channelData[i] = sample;
      }
    }
  }

  // ── 4. Master Volume Gain Staging ─────────────────────────────────────────
  if (masterGain != 1.0f) {
    buffer.applyGain(startSample, numSamples, masterGain);
  }

  // ── 5. Peak VU Meter Level Detection (Atomic smoothing) ───────────────────
  float maxL = buffer.getMagnitude(0, startSample, numSamples);
  float maxR = (numChannels > 1)
                   ? buffer.getMagnitude(1, startSample, numSamples)
                   : maxL;

  // Smooth peak decay (0.95 factor per block)
  float currentL = peakLevelL.load();
  float currentR = peakLevelR.load();

  peakLevelL.store(juce::jmax(maxL, currentL * 0.95f));
  peakLevelR.store(juce::jmax(maxR, currentR * 0.95f));
}
