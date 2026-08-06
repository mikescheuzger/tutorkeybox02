#include "FXChannel.h"

// =============================================================================
// Constructor — Sets default FX parameters
// =============================================================================
FXChannel::FXChannel() {
    reverbParams.roomSize   = 0.5f;
    reverbParams.damping    = 0.5f;
    reverbParams.wetLevel   = 0.33f;
    reverbParams.dryLevel   = 1.0f;
    reverbParams.width      = 1.0f;
    reverbParams.freezeMode = 0.0f;
    reverb.setParameters(reverbParams);
}

// =============================================================================
// prepareToPlay — Prepares DSP modules and pre-allocates buffers
// =============================================================================
void FXChannel::prepareToPlay(double sampleRate, int samplesPerBlock) {
    currentSampleRate = sampleRate;
    int safeBlockSize = juce::jmax(512, samplesPerBlock);

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32)safeBlockSize;
    spec.numChannels = 1; // Single channel per mono delay line object

    // Prepare Reverb (Stereo 2 channels)
    juce::dsp::ProcessSpec reverbSpec = spec;
    reverbSpec.numChannels = 2;
    reverb.prepare(reverbSpec);
    reverb.reset();

    // Prepare Delay Lines (1 channel each for Left and Right)
    delayLineL.prepare(spec);
    delayLineR.prepare(spec);

    // Set delay line length FIRST so read pointers are positioned before reset()
    setDelayTimeMs(targetDelayTimeMs);

    // Clear entire delay buffer memory cleanly
    delayLineL.reset();
    delayLineR.reset();

    // Pre-allocate temporary delay processing buffer (Zero Heap Allocation)
    delayBuffer.setSize(2, safeBlockSize, false, false, true);
    delayBuffer.clear();
}

void FXChannel::reset() {
    reverb.reset();
    delayLineL.reset();
    delayLineR.reset();
    delayBuffer.clear();
}

// =============================================================================
// Reverb Parameter Setters
// =============================================================================
void FXChannel::setReverbRoomSize(float size0to1) {
    reverbParams.roomSize = juce::jlimit(0.0f, 1.0f, size0to1);
    reverb.setParameters(reverbParams);
}

void FXChannel::setReverbDamping(float damping0to1) {
    reverbParams.damping = juce::jlimit(0.0f, 1.0f, damping0to1);
    reverb.setParameters(reverbParams);
}

void FXChannel::setReverbWetLevel(float wet0to1) {
    reverbParams.wetLevel = juce::jlimit(0.0f, 1.0f, wet0to1);
    reverb.setParameters(reverbParams);
}

void FXChannel::setReverbDryLevel(float dry0to1) {
    reverbParams.dryLevel = juce::jlimit(0.0f, 1.0f, dry0to1);
    reverb.setParameters(reverbParams);
}

// =============================================================================
// Delay Parameter Setters
// =============================================================================
void FXChannel::setDelayTimeMs(float delayMs) {
    targetDelayTimeMs = juce::jlimit(1.0f, 2000.0f, delayMs);
    if (currentSampleRate > 0.0) {
        float delaySamples = (targetDelayTimeMs * 0.001f) * (float)currentSampleRate;
        delayLineL.setDelay(delaySamples);
        delayLineR.setDelay(delaySamples);
    }
}

void FXChannel::setDelayFeedback(float feedback0to1) {
    delayFeedback = juce::jlimit(0.0f, 0.95f, feedback0to1);
}

void FXChannel::setDelayWetLevel(float wet0to1) {
    delayWetLevel = juce::jlimit(0.0f, 1.0f, wet0to1);
}

// =============================================================================
// processBlock — Main Audio Processing for FX Bus
// =============================================================================
void FXChannel::processBlock(juce::AudioBuffer<float>& auxBuffer,
                             juce::AudioBuffer<float>& outputBuffer,
                             int startSample, int numSamples) {

    if (numSamples <= 0 || fxOutputGain <= 0.0001f)
        return;

    const int numChannels = juce::jmin(2, auxBuffer.getNumChannels());

    // ── 1. Process Delay Line ────────────────────────────────────────────────
    if (delayEnabled) {
        delayBuffer.clear(0, numSamples);

        const float* auxL = auxBuffer.getReadPointer(0, startSample);
        const float* auxR = (numChannels > 1) ? auxBuffer.getReadPointer(1, startSample) : auxL;

        float* dOutL = delayBuffer.getWritePointer(0, 0);
        float* dOutR = delayBuffer.getWritePointer(1, 0);

        for (int i = 0; i < numSamples; ++i) {
            float inL = auxL[i];
            float inR = auxR[i];

            // Push input sample FIRST into delay line
            delayLineL.pushSample(0, inL);
            delayLineR.pushSample(0, inR);

            // Pop delayed sample
            float delayedL = delayLineL.popSample(0);
            float delayedR = delayLineR.popSample(0);

            // Add feedback back into delay line
            if (delayFeedback > 0.001f) {
                delayLineL.pushSample(0, delayedL * delayFeedback);
                delayLineR.pushSample(0, delayedR * delayFeedback);
                delayLineL.popSample(0);
                delayLineR.popSample(0);
            }

            // Write wet delay to temporary buffer
            dOutL[i] = delayedL * delayWetLevel;
            dOutR[i] = delayedR * delayWetLevel;
        }

        // Add wet delay signal to aux buffer
        for (int ch = 0; ch < numChannels; ++ch) {
            auxBuffer.addFrom(ch, startSample, delayBuffer, ch, 0, numSamples, 1.0f);
        }
    }

    // ── 2. Process Stereo Reverb ─────────────────────────────────────────────
    if (reverbEnabled) {
        juce::dsp::AudioBlock<float> block(auxBuffer.getArrayOfWritePointers(),
                                           (size_t)numChannels,
                                           (size_t)startSample,
                                           (size_t)numSamples);
        juce::dsp::ProcessContextReplacing<float> context(block);
        reverb.process(context);
    }

    // ── 3. Sum FX Output into Main Output Buffer ─────────────────────────────
    for (int ch = 0; ch < outputBuffer.getNumChannels(); ++ch) {
        int srcCh = juce::jmin(ch, numChannels - 1);
        outputBuffer.addFrom(ch, startSample, auxBuffer, srcCh, startSample, numSamples, fxOutputGain);
    }
}
