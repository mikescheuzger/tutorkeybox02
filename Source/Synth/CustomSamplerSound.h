#pragma once
#include "SampleHeader.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_core/juce_core.h>
#include <memory>
#include <vector>

/**
 * CustomSamplerSound — one individual sample variant.
 *
 * Each object holds the audio data for exactly one recorded take:
 *   • one velocity zone   (e.g. MF, zone 5, vel 57–70)
 *   • one pedal state     (NoPedal or WithPedal)
 *   • one round-robin take (e.g. rr2)
 *
 * Multiple CustomSamplerSound objects for the same key zone are stored in the
 * LayeredSynth lookup grid. LayeredSynth picks the right variant at note-on
 * time based on current pedal state and the round-robin counter.
 *
 * Audio is split into two regions:
 *   1. Attack (RAM): First ~150ms pre-loaded into RAM for 0-latency onset
 *   2. Tail (mmap): Full sample tail streamed via zero-copy mmap
 */
class CustomSamplerSound : public juce::SynthesiserSound {
public:
    CustomSamplerSound(const SampleEntry& entry,
                       const juce::AudioBuffer<float>& attackRamBuffer,
                       const std::vector<const float*>& tailChannelPointers,
                       int totalNumSamples,
                       double sampleRate,
                       std::shared_ptr<juce::MemoryMappedFile> mappedFile = nullptr);

    ~CustomSamplerSound() override = default;

    bool appliesToNote(int midiNoteNumber) override;
    bool appliesToChannel(int /*midiChannel*/) override { return true; }
    bool appliesToVelocity(int midiVelocity);

    const SampleEntry& getEntry() const { return entry; }
    SampleType getSampleType() const { return entry.sampleType; }
    VelocityLayer getVelocityLayer() const { return entry.velocityLayer; }
    PedalState getPedalState() const { return entry.pedalState; }
    uint8_t getRoundRobinIndex() const { return entry.roundRobinIndex; }
    uint8_t getRoundRobinCount() const { return entry.roundRobinCount; }

    const juce::AudioBuffer<float>& getAttackBuffer() const { return attackRamBuffer; }
    const float* const* getTailChannelPointers() const { return tailChannelPointers.data(); }
    int getTailNumSamples() const { return totalNumSamples; }
    int getNumChannels() const { return (int)entry.numChannels; }
    double getSampleRate() const { return sampleRate; }

private:
    SampleEntry entry;
    juce::AudioBuffer<float> attackRamBuffer;
    std::vector<const float*> tailChannelPointers;
    int totalNumSamples;
    double sampleRate;
    std::shared_ptr<juce::MemoryMappedFile> mappedFile;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CustomSamplerSound)
};
