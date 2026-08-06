#include "CustomSamplerSound.h"

// =============================================================================
// Constructor — Initialises CustomSamplerSound with attack RAM & mmap tail pointers
// =============================================================================
CustomSamplerSound::CustomSamplerSound(const SampleEntry& e,
                                       const juce::AudioBuffer<float>& attackBuffer,
                                       const std::vector<const float*>& tailPointers,
                                       int totalSamples,
                                       double sRate,
                                       std::shared_ptr<juce::MemoryMappedFile> mFile)
    : entry(e), attackRamBuffer(attackBuffer), tailChannelPointers(tailPointers),
      totalNumSamples(totalSamples), sampleRate(sRate), mappedFile(mFile) {}

// =============================================================================
// MIDI Zone Matching Helpers
// =============================================================================
bool CustomSamplerSound::appliesToNote(int midiNoteNumber) {
    return midiNoteNumber >= entry.keyLow && midiNoteNumber <= entry.keyHigh;
}

bool CustomSamplerSound::appliesToVelocity(int midiVelocity) {
    return midiVelocity >= entry.velZoneLow && midiVelocity <= entry.velZoneHigh;
}
