#include "LayeredSynth.h"

// =============================================================================
// Constructor — Pre-creates 32 CustomSamplerVoice instances per layer
// =============================================================================
LayeredSynth::LayeredSynth(MidiState& stateToUpdate)
    : midiState(stateToUpdate) {
    for (int layerIdx = 0; layerIdx < NUM_LAYERS; ++layerIdx) {
        auto& layer = layers[(size_t)layerIdx];
        for (int v = 0; v < VOICES_PER_LAYER; ++v) {
            layer.synth.addVoice(new CustomSamplerVoice(&stateToUpdate));
        }

        // Initialize state variable filter defaults
        layer.filter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
        layer.filter.setCutoffFrequency(layer.filterCutoffHz);
        layer.filter.setResonance(layer.filterResonanceQ);
    }
}

// =============================================================================
// prepareToPlay — Sets sample rate for synth voices, DSP filters, and buffers
// =============================================================================
void LayeredSynth::prepareToPlay(double sampleRate, int samplesPerBlock) {
    int safeBlockSize = juce::jmax(512, samplesPerBlock);

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32)safeBlockSize;
    spec.numChannels = 2;

    for (int layerIdx = 0; layerIdx < NUM_LAYERS; ++layerIdx) {
        auto& layer = layers[(size_t)layerIdx];

        layer.synth.setCurrentPlaybackSampleRate(sampleRate);

        // Notify voices of sample rate for ADSR calculations
        for (int v = 0; v < layer.synth.getNumVoices(); ++v) {
            if (auto* voice = dynamic_cast<CustomSamplerVoice*>(layer.synth.getVoice(v))) {
                voice->prepare(sampleRate);
            }
        }

        // Prepare resonant filter
        layer.filter.prepare(spec);
        layer.filter.reset();

        // Pre-allocate temp render buffer capacity ONCE (Zero Heap Allocation on audio thread)
        layer.tempBuffer.setSize(2, safeBlockSize, false, false, true);
        layer.tempBuffer.clear();
    }
}

// =============================================================================
// getNumActiveVoices — Counts total active playing voices across all 4 layers
// =============================================================================
int LayeredSynth::getNumActiveVoices() const {
    int activeCount = 0;
    for (int layerIdx = 0; layerIdx < NUM_LAYERS; ++layerIdx) {
        const auto& layer = layers[(size_t)layerIdx];
        for (int v = 0; v < layer.synth.getNumVoices(); ++v) {
            if (layer.synth.getVoice(v)->isVoiceActive()) {
                ++activeCount;
            }
        }
    }
    return activeCount;
}

// =============================================================================
// Variant Selection Helper — Selects best matching round-robin and pedal variant
// =============================================================================
LayeredSynth::SoundPtr LayeredSynth::selectBestVariant(
    const std::vector<SoundPtr>& candidates,
    PedalState currentPedalState,
    uint8_t noteNumber,
    uint8_t& rrCounterRef) {

    if (candidates.empty())
        return nullptr;

    // 1. Filter candidates matching current pedal state
    std::vector<SoundPtr> matchingPedal;
    for (const auto& sound : candidates) {
        if (auto* cs = dynamic_cast<CustomSamplerSound*>(sound.get())) {
            if (cs->getPedalState() == currentPedalState) {
                matchingPedal.push_back(sound);
            }
        }
    }

    // Fallback to all candidates if no exact pedalState match exists
    const auto& pool = matchingPedal.empty() ? candidates : matchingPedal;

    // 2. Select variant using per-note Round-Robin counter
    uint8_t targetRR = rrCounterRef;
    SoundPtr selected = pool[0]; // fallback

    for (const auto& sound : pool) {
        if (auto* cs = dynamic_cast<CustomSamplerSound*>(sound.get())) {
            if (cs->getRoundRobinIndex() == targetRR) {
                selected = sound;
                break;
            }
        }
    }

    // Advance round-robin counter for next note-on
    if (auto* selectedCS = dynamic_cast<CustomSamplerSound*>(selected.get())) {
        uint8_t rrCount = selectedCS->getRoundRobinCount();
        if (rrCount > 1) {
            rrCounterRef = (uint8_t)((targetRR + 1) % rrCount);
        }
    }

    return selected;
}

// =============================================================================
// renderNextBlock — Main Real-Time Audio Callback
// =============================================================================
void LayeredSynth::renderNextBlock(juce::AudioBuffer<float>& outputBuffer,
                                   juce::MidiBuffer& midiMessages,
                                   int startSample, int numSamples,
                                   juce::AudioBuffer<float>* auxOutputBuffer) {

    // ── Process MIDI CC64 Pedal Events & Build Layer MIDI Buffers ─────────────
    for (const auto metadata : midiMessages) {
        auto msg = metadata.getMessage();

        // Sustain Pedal (CC64) Tracking
        if (msg.isController() && msg.getControllerNumber() == 64) {
            bool newPedalDown = (msg.getControllerValue() >= 64);
            if (newPedalDown != isPedalDown) {
                isPedalDown = newPedalDown;

                // Trigger Pedal Noise samples on pedal state transition
                PedalState pState = isPedalDown ? PedalState::WithPedal : PedalState::NoPedal;
                for (int layerIdx = 0; layerIdx < NUM_LAYERS; ++layerIdx) {
                    auto& layer = layers[(size_t)layerIdx];
                    if (layer.muted) continue;

                    const auto& pedalPool = isPedalDown ? layer.pedalDownSounds : layer.pedalUpSounds;
                    if (!pedalPool.empty()) {
                        uint8_t dummyRR = 0;
                        auto sound = selectBestVariant(pedalPool, pState, 60, dummyRR);
                        if (sound != nullptr) {
                            juce::SynthesiserVoice* voiceToUse = nullptr;
                            for (int v = 0; v < layer.synth.getNumVoices(); ++v) {
                                if (!layer.synth.getVoice(v)->isVoiceActive()) {
                                    voiceToUse = layer.synth.getVoice(v);
                                    break;
                                }
                            }
                            if (voiceToUse != nullptr) {
                                layer.synth.triggerVoice(voiceToUse, sound.get(), 1, 60, 0.8f);
                            }
                        }
                    }
                }
            }
        }
    }

    // ── Render Each Layer ─────────────────────────────────────────────────────
    PedalState activePedalState = isPedalDown ? PedalState::WithPedal : PedalState::NoPedal;

    for (int layerIdx = 0; layerIdx < NUM_LAYERS; ++layerIdx) {
        auto& layer = layers[(size_t)layerIdx];
        if (layer.muted)
            continue;

        juce::MidiBuffer layerMidi;

        for (const auto metadata : midiMessages) {
            auto msg = metadata.getMessage();
            int samplePos = metadata.samplePosition;

            if (msg.isNoteOn()) {
                int note = juce::jlimit(0, 127, msg.getNoteNumber());
                int velInt = juce::jlimit(0, 127, juce::roundToInt(msg.getFloatVelocity() * 127.0f));

                const auto& candidates = layer.soundLookupGrid[(size_t)note][(size_t)velInt];
                if (!candidates.empty()) {
                    if (auto matchingSound = selectBestVariant(candidates, activePedalState, (uint8_t)note, layer.roundRobinCounters[(size_t)note])) {
                        // 1. First look for an idle (free) voice
                        juce::SynthesiserVoice* voiceToUse = nullptr;
                        for (int v = 0; v < layer.synth.getNumVoices(); ++v) {
                            if (!layer.synth.getVoice(v)->isVoiceActive()) {
                                voiceToUse = layer.synth.getVoice(v);
                                break;
                            }
                        }

                        // 2. If all voices are busy, steal the QUIETEST active voice (Quietest + LRU Stealing):
                        if (voiceToUse == nullptr && layer.synth.getNumVoices() > 0) {
                            float lowestVolume = 999999.0f;
                            for (int v = 0; v < layer.synth.getNumVoices(); ++v) {
                                if (auto* csv = dynamic_cast<CustomSamplerVoice*>(layer.synth.getVoice(v))) {
                                    float level = csv->getCurrentLevel();
                                    if (level < lowestVolume) {
                                        lowestVolume = level;
                                        voiceToUse = csv;
                                    }
                                }
                            }
                        }

                        if (voiceToUse != nullptr) {
                            layer.synth.triggerVoice(voiceToUse, matchingSound.get(),
                                                     msg.getChannel(), note,
                                                     msg.getFloatVelocity());
                        }

                        if (auto* cs = dynamic_cast<CustomSamplerSound*>(matchingSound.get())) {
                            juce::String sName = cs->getEntry().name;
                            int vLow = (int)cs->getEntry().velZoneLow;
                            int vHigh = (int)cs->getEntry().velZoneHigh;
                            juce::MessageManager::callAsync([note, velInt, vLow, vHigh, sName] {
                                std::cout << "[LIVE SAMPLE] Note: " << note << " | Vel: " << velInt
                                          << " | File: " << sName.toStdString()
                                          << " | VelZone: " << vLow << ".." << vHigh << std::endl;
                            });
                        }
                    }
                }
            } else if (msg.isNoteOff()) {
                layerMidi.addEvent(msg, samplePos);

                // Trigger Release Trigger samples if pedal is UP and present for this note
                int note = juce::jlimit(0, 127, msg.getNoteNumber());
                const auto& relPool = layer.releaseSounds[(size_t)note];
                if (!isPedalDown && !relPool.empty()) {
                    uint8_t dummyRR = 0;
                    auto relSound = selectBestVariant(relPool, activePedalState, (uint8_t)note, dummyRR);
                    if (relSound != nullptr) {
                        juce::SynthesiserVoice* voiceToUse = nullptr;
                        for (int v = 0; v < layer.synth.getNumVoices(); ++v) {
                            if (!layer.synth.getVoice(v)->isVoiceActive()) {
                                voiceToUse = layer.synth.getVoice(v);
                                break;
                            }
                        }
                        if (voiceToUse != nullptr) {
                            layer.synth.triggerVoice(voiceToUse, relSound.get(),
                                                     msg.getChannel(), note,
                                                     msg.getFloatVelocity() * 0.25f);
                        }
                    }
                }
            } else {
                layerMidi.addEvent(msg, samplePos);
            }
        }

        // Render audio voices into pre-allocated temp buffer
        layer.tempBuffer.clear(0, numSamples);
        layer.synth.renderNextBlock(layer.tempBuffer, layerMidi, 0, numSamples);

        // Apply per-layer Resonant Low-Pass Filter
        juce::dsp::AudioBlock<float> block(layer.tempBuffer.getArrayOfWritePointers(),
                                           (size_t)layer.tempBuffer.getNumChannels(),
                                           (size_t)0,
                                           (size_t)numSamples);
        layer.filter.process(juce::dsp::ProcessContextReplacing<float>(block));

        // Mix to Main Output Buffer
        for (int ch = 0; ch < outputBuffer.getNumChannels(); ++ch) {
            outputBuffer.addFrom(ch, startSample, layer.tempBuffer, ch, 0, numSamples, layer.volumeGain);
        }

        // Mix to AUX Send Buffer (if provided and send gain > 0)
        if (auxOutputBuffer != nullptr && layer.auxSendGain > 0.0f) {
            for (int ch = 0; ch < auxOutputBuffer->getNumChannels(); ++ch) {
                auxOutputBuffer->addFrom(ch, startSample, layer.tempBuffer, ch, 0, numSamples, layer.auxSendGain);
            }
        }
    }
}

// =============================================================================
// Sound Registration Methods
// =============================================================================
void LayeredSynth::addNoteOnSoundToLayer(int layerIndex, juce::SynthesiserSound::Ptr sound) {
    if (layerIndex >= 0 && layerIndex < NUM_LAYERS && sound != nullptr) {
        auto& layer = layers[(size_t)layerIndex];
        layer.synth.addSound(sound);

        if (auto* cs = dynamic_cast<CustomSamplerSound*>(sound.get())) {
            const auto& entry = cs->getEntry();
            int kLow = juce::jlimit(0, 127, (int)entry.keyLow);
            int kHigh = juce::jlimit(0, 127, (int)entry.keyHigh);
            int vLow = juce::jlimit(0, 127, (int)entry.velZoneLow);
            int vHigh = juce::jlimit(0, 127, (int)entry.velZoneHigh);

            for (int n = kLow; n <= kHigh; ++n) {
                for (int v = vLow; v <= vHigh; ++v) {
                    layer.soundLookupGrid[(size_t)n][(size_t)v].push_back(sound);
                }
            }

            std::cout << "[GRID REGISTRATION] Sound: " << entry.name 
                      << " | NoteRange: " << kLow << ".." << kHigh 
                      << " | VelRange: " << vLow << ".." << vHigh << std::endl;
        }
    }
}

void LayeredSynth::addReleaseSoundToLayer(int layerIndex, juce::SynthesiserSound::Ptr sound) {
    if (layerIndex >= 0 && layerIndex < NUM_LAYERS && sound != nullptr) {
        auto& layer = layers[(size_t)layerIndex];

        if (auto* cs = dynamic_cast<CustomSamplerSound*>(sound.get())) {
            const auto& entry = cs->getEntry();
            int kLow = juce::jlimit(0, 127, (int)entry.keyLow);
            int kHigh = juce::jlimit(0, 127, (int)entry.keyHigh);

            for (int n = kLow; n <= kHigh; ++n) {
                layer.releaseSounds[(size_t)n].push_back(sound);
            }
        }
    }
}

void LayeredSynth::addPedalSoundToLayer(int layerIndex, juce::SynthesiserSound::Ptr sound) {
    if (layerIndex >= 0 && layerIndex < NUM_LAYERS && sound != nullptr) {
        auto& layer = layers[(size_t)layerIndex];

        if (auto* cs = dynamic_cast<CustomSamplerSound*>(sound.get())) {
            if (cs->getSampleType() == SampleType::PedalDown) {
                layer.pedalDownSounds.push_back(sound);
            } else if (cs->getSampleType() == SampleType::PedalUp) {
                layer.pedalUpSounds.push_back(sound);
            }
        }
    }
}

void LayeredSynth::addSoundToLayer(int layerIndex, juce::SynthesiserSound::Ptr sound) {
    if (auto* cs = dynamic_cast<CustomSamplerSound*>(sound.get())) {
        switch (cs->getSampleType()) {
            case SampleType::NoteOn:         addNoteOnSoundToLayer(layerIndex, sound); break;
            case SampleType::ReleaseTrigger: addReleaseSoundToLayer(layerIndex, sound); break;
            case SampleType::PedalDown:
            case SampleType::PedalUp:        addPedalSoundToLayer(layerIndex, sound); break;
        }
    }
}

void LayeredSynth::clearLayer(int layerIndex) {
    if (layerIndex >= 0 && layerIndex < NUM_LAYERS) {
        auto& layer = layers[(size_t)layerIndex];
        layer.synth.clearSounds();

        for (int n = 0; n < 128; ++n) {
            for (int v = 0; v < 128; ++v) {
                layer.soundLookupGrid[(size_t)n][(size_t)v].clear();
            }
            layer.releaseSounds[(size_t)n].clear();
            layer.roundRobinCounters[(size_t)n] = 0;
        }
        layer.pedalDownSounds.clear();
        layer.pedalUpSounds.clear();
    }
}

void LayeredSynth::clearAllLayers() {
    for (int layerIdx = 0; layerIdx < NUM_LAYERS; ++layerIdx) {
        clearLayer(layerIdx);
    }
}

// =============================================================================
// Layer Control Setters
// =============================================================================
void LayeredSynth::setLayerVolume(int layerIndex, float gainLinear) {
    if (layerIndex >= 0 && layerIndex < NUM_LAYERS) {
        layers[(size_t)layerIndex].volumeGain = gainLinear;
    }
}

void LayeredSynth::setLayerMute(int layerIndex, bool isMuted) {
    if (layerIndex >= 0 && layerIndex < NUM_LAYERS) {
        layers[(size_t)layerIndex].muted = isMuted;
    }
}

void LayeredSynth::setLayerSampleInputGain(int layerIndex, float gainLinear) {
    if (layerIndex >= 0 && layerIndex < NUM_LAYERS) {
        auto& layer = layers[(size_t)layerIndex];
        layer.sampleInputGain = gainLinear;

        for (int v = 0; v < layer.synth.getNumVoices(); ++v) {
            if (auto* voice = dynamic_cast<CustomSamplerVoice*>(layer.synth.getVoice(v))) {
                voice->setSampleInputGain(gainLinear);
            }
        }
    }
}

void LayeredSynth::setLayerAdsr(int layerIndex, const AdsrParams& params) {
    if (layerIndex >= 0 && layerIndex < NUM_LAYERS) {
        auto& layer = layers[(size_t)layerIndex];
        layer.adsrParams = params;

        for (int v = 0; v < layer.synth.getNumVoices(); ++v) {
            if (auto* voice = dynamic_cast<CustomSamplerVoice*>(layer.synth.getVoice(v))) {
                voice->setAdsrParams(params);
            }
        }
    }
}

void LayeredSynth::setLayerFilterCutoff(int layerIndex, float cutoffHz) {
    if (layerIndex >= 0 && layerIndex < NUM_LAYERS) {
        auto& layer = layers[(size_t)layerIndex];
        layer.filterCutoffHz = juce::jlimit(20.0f, 20000.0f, cutoffHz);
        layer.filter.setCutoffFrequency(layer.filterCutoffHz);
    }
}

void LayeredSynth::setLayerFilterResonance(int layerIndex, float resonanceQ) {
    if (layerIndex >= 0 && layerIndex < NUM_LAYERS) {
        auto& layer = layers[(size_t)layerIndex];
        layer.filterResonanceQ = juce::jlimit(0.1f, 10.0f, resonanceQ);
        layer.filter.setResonance(layer.filterResonanceQ);
    }
}

void LayeredSynth::setLayerAuxSend(int layerIndex, float auxGainLinear) {
    if (layerIndex >= 0 && layerIndex < NUM_LAYERS) {
        layers[(size_t)layerIndex].auxSendGain = juce::jmax(0.0f, auxGainLinear);
    }
}
