#include "CustomSamplerVoice.h"

// =============================================================================
// Construction
// =============================================================================
CustomSamplerVoice::CustomSamplerVoice(MidiState* stateToUpdate)
    : midiState(stateToUpdate) {}

// =============================================================================
// prepare — call this when the audio device opens or sample rate changes
// =============================================================================
void CustomSamplerVoice::prepare(double newSampleRate) {
    currentSampleRate = newSampleRate;
    computeAdsrRates();
}

// =============================================================================
// computeAdsrRates — converts ms parameters to per-sample increment values
// Called once on prepare() and again whenever AdsrParams change mid-session
// =============================================================================
void CustomSamplerVoice::computeAdsrRates() {
    if (currentSampleRate <= 0.0) return;

    // Attack: ramp from 0.0 to 1.0 over attackMs milliseconds
    float attackSamples  = (float)(adsrParams.attackMs  * 0.001 * currentSampleRate);
    adsrAttackInc  = (attackSamples  > 0.0f) ? (1.0f / attackSamples)  : 1.0f;

    // Decay: ramp from 1.0 down to sustainLevel over decayMs milliseconds
    float decaySamples   = (float)(adsrParams.decayMs   * 0.001 * currentSampleRate);
    float decayDelta = 1.0f - adsrParams.sustainLevel;
    adsrDecayDec   = (decaySamples   > 0.0f && decayDelta > 0.0f)
                   ? (decayDelta / decaySamples) : 0.0f;

    // Release: ramp from current level to 0.0 over releaseMs milliseconds
    // We compute this relative to full amplitude (1.0) and scale at release time
    float releaseSamples = (float)(adsrParams.releaseMs * 0.001 * currentSampleRate);
    adsrReleaseDec = (releaseSamples > 0.0f) ? (1.0f / releaseSamples) : 1.0f;
}

// =============================================================================
// canPlaySound
// =============================================================================
bool CustomSamplerVoice::canPlaySound(juce::SynthesiserSound* sound) {
    return dynamic_cast<const CustomSamplerSound*>(sound) != nullptr;
}

// =============================================================================
// startNote — triggered when a MIDI note-on arrives and this voice is selected
// =============================================================================
void CustomSamplerVoice::startNote(int midiNoteNumber, float velocity,
                                   juce::SynthesiserSound* sound,
                                   int /*currentPitchWheelPosition*/) {
    activeSound = static_cast<const CustomSamplerSound*>(sound);

    if (activeSound == nullptr) {
        clearCurrentNote();
        return;
    }

    const auto& entry = activeSound->getEntry();

    // ── Pitch ratio — always pitches DOWN from root (root is keyHigh) ─────────
    // If midiNoteNumber == entry.rootNote → pitchRatio = 1.0 (natural playback)
    // If midiNoteNumber < entry.rootNote  → pitchRatio < 1.0 (slower = lower pitch)
    double noteHz = juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber);
    double rootHz = juce::MidiMessage::getMidiNoteInHertz(entry.rootNote);
    double outputSR = (getSampleRate() > 0.0) ? getSampleRate() : currentSampleRate;

    pitchRatio = (rootHz > 0.0 && outputSR > 0.0)
               ? (noteHz / rootHz) * ((double)entry.sampleRate / outputSR)
               : 1.0;

    // ── Voice Stealing Crossfade De-Clicker Capture ───────────────────────────
    if (adsrStage != AdsrStage::Idle) {
        stolenLastL = lastOutputL;
        stolenLastR = lastOutputR;
        declickTotalSamples = (int)(0.002 * outputSR);
        if (declickTotalSamples < 1) declickTotalSamples = 1;
        declickSamplesRemaining = declickTotalSamples;
    } else {
        declickSamplesRemaining = 0;
        stolenLastL = 0.0f;
        stolenLastR = 0.0f;
    }

    // ── Static gain: velocity × zone multiplier × user input gain ─────────────
    // Computed once here to avoid per-sample multiplications in renderNextBlock
    staticGain = velocity * entry.zoneGainMultiplier * sampleInputGain;
    lgain = 1.0f;
    rgain = 1.0f;

    // ── Reset playback position ───────────────────────────────────────────────
    sourceSamplePosition = 0.0;

    // ── Start ADSR from Attack stage ─────────────────────────────────────────
    // Recompute rates in case params changed since last prepare()
    currentSampleRate = outputSR;
    computeAdsrRates();

    adsrLevel = 0.0f;
    adsrStage = AdsrStage::Attack;

    // ── Report to MIDI console log ────────────────────────────────────────────
    if (midiState != nullptr) {
        midiState->updateSampleInspector(entry.name, entry.rootNote,
                                         entry.keyLow, entry.keyHigh,
                                         entry.velZoneLow, entry.velZoneHigh);
    }
}

// =============================================================================
// stopNote — triggered on MIDI note-off
// =============================================================================
void CustomSamplerVoice::stopNote(float /*velocity*/, bool allowTailOff) {
    if (allowTailOff && adsrStage != AdsrStage::Idle) {
        // Enter Release — decrement from current adsrLevel (not necessarily 1.0)
        // We scale adsrReleaseDec so it reaches 0 over the full releaseMs
        // regardless of where the envelope is when the key lifts
        if (adsrLevel > 0.0f)
            adsrReleaseDec = adsrLevel * adsrReleaseDec;  // scaled to current level

        adsrStage = AdsrStage::Release;
    } else {
        // Hard stop — note cut immediately (e.g. voice stealing)
        adsrStage = AdsrStage::Idle;
        clearCurrentNote();
        sourceSamplePosition = 0.0;
        activeSound = nullptr;
    }
}

void CustomSamplerVoice::pitchWheelMoved(int /*v*/) {}
void CustomSamplerVoice::controllerMoved(int /*cc*/, int /*val*/) {}

// =============================================================================
// renderNextBlock — the real-time audio render callback
// Called on the audio thread: NO allocations, NO locks, NO JUCE message thread
// =============================================================================
void CustomSamplerVoice::renderNextBlock(juce::AudioBuffer<float>& outputBuffer,
                                         int startSample, int numSamples) {
    if (activeSound == nullptr || adsrStage == AdsrStage::Idle)
        return;

    // Audio source regions
    const auto& attackBuffer      = activeSound->getAttackBuffer();
    const int   attackNumSamples  = attackBuffer.getNumSamples();
    const float* const* attackCh  = attackBuffer.getArrayOfReadPointers();

    const float* const* tailCh    = activeSound->getTailChannelPointers();
    const int   tailNumSamples    = activeSound->getTailNumSamples();
    const int   numSrcChannels    = activeSound->getNumChannels();

    // Output write pointers
    float* outL = outputBuffer.getWritePointer(0, startSample);
    float* outR = (outputBuffer.getNumChannels() > 1)
                ? outputBuffer.getWritePointer(1, startSample)
                : nullptr;

    for (int i = 0; i < numSamples; ++i) {
        // ── ADSR envelope tick ────────────────────────────────────────────────
        switch (adsrStage) {
            case AdsrStage::Attack:
                adsrLevel += adsrAttackInc;
                if (adsrLevel >= 1.0f) {
                    adsrLevel = 1.0f;
                    adsrStage = AdsrStage::Decay;
                }
                break;

            case AdsrStage::Decay:
                adsrLevel -= adsrDecayDec;
                if (adsrLevel <= adsrParams.sustainLevel) {
                    adsrLevel = adsrParams.sustainLevel;
                    adsrStage = AdsrStage::Sustain;
                }
                break;

            case AdsrStage::Sustain:
                // Level held constant — nothing to update
                break;

            case AdsrStage::Release:
                adsrLevel -= adsrReleaseDec;
                if (adsrLevel <= 0.0f) {
                    adsrLevel = 0.0f;
                    adsrStage = AdsrStage::Idle;
                    clearCurrentNote();
                    activeSound = nullptr;
                    return;  // Done — exit render loop early
                }
                break;

            case AdsrStage::Idle:
            default:
                return;
        }

        // ── Sample read with linear interpolation ─────────────────────────────
        int   posInt = (int)sourceSamplePosition;
        float alpha  = (float)(sourceSamplePosition - posInt);

        float sampleL = 0.0f;
        float sampleR = 0.0f;

        if (posInt < attackNumSamples - 1) {
            // Phase 1: RAM attack buffer — zero-glitch onset
            sampleL = attackCh[0][posInt] + alpha * (attackCh[0][posInt + 1] - attackCh[0][posInt]);
            sampleR = (numSrcChannels > 1)
                    ? attackCh[1][posInt] + alpha * (attackCh[1][posInt + 1] - attackCh[1][posInt])
                    : sampleL;
        } else if (posInt < tailNumSamples - 1) {
            // Phase 2: mmap tail — power-cut safe, zero heap
            sampleL = tailCh[0][posInt] + alpha * (tailCh[0][posInt + 1] - tailCh[0][posInt]);
            sampleR = (numSrcChannels > 1)
                    ? tailCh[1][posInt] + alpha * (tailCh[1][posInt + 1] - tailCh[1][posInt])
                    : sampleL;
        } else {
            // Sample finished — end voice cleanly
            adsrStage = AdsrStage::Idle;
            clearCurrentNote();
            activeSound = nullptr;
            return;
        }

        // ── Apply gain chain: staticGain (zone × inputGain) × ADSR ───────────
        float finalGain = staticGain * adsrLevel;
        sampleL *= (lgain * finalGain);
        sampleR *= (rgain * finalGain);

        // ── Voice Stealing Crossfade De-Clicker (2ms fade out for stolen voice) ─
        if (declickSamplesRemaining > 0) {
            float declickFade = (float)declickSamplesRemaining / (float)declickTotalSamples;
            sampleL += stolenLastL * declickFade;
            sampleR += stolenLastR * declickFade;
            --declickSamplesRemaining;
        }

        lastOutputL = sampleL;
        lastOutputR = sampleR;

        outL[i] += sampleL;
        if (outR != nullptr) outR[i] += sampleR;

        sourceSamplePosition += pitchRatio;
    }
}
