#include "SampleContainerReader.h"
#include "LayeredSynth.h"

bool SampleContainerReader::loadContainerFile(const juce::File& file,
                                              LayeredSynth&     synthTarget,
                                              int               targetLayerIndex) {
    if (!file.existsAsFile()) {
        juce::Logger::writeToLog("SampleContainerReader Error: File does not exist — " + file.getFullPathName());
        return false;
    }

    // ── Check if file is an SFZ Descriptor File (.sfz) ────────────────────────
    if (file.getFileExtension().equalsIgnoreCase(".sfz")) {
        return SfzReader::loadSfzFile(file, synthTarget, targetLayerIndex);
    }

    // ── Zero-copy memory-map the entire .bin container file ───────────────────
    auto mappedFile = std::make_shared<juce::MemoryMappedFile>(file, juce::MemoryMappedFile::readOnly);

    if (mappedFile->getSize() < (int64_t)sizeof(ContainerHeader)) {
        juce::Logger::writeToLog("SampleContainerReader Error: File empty or mapping failed — " + file.getFullPathName());
        return false;
    }

    const char* basePtr = static_cast<const char*>(mappedFile->getData());
    const auto* header = reinterpret_cast<const ContainerHeader*>(basePtr);

    if (header->magic[0] != TKB_Magic[0] || header->magic[1] != TKB_Magic[1] ||
        header->magic[2] != TKB_Magic[2] || header->magic[3] != TKB_Magic[3]) {
        juce::Logger::writeToLog("SampleContainerReader Error: Invalid magic signature! File is not a TKB2 container.");
        return false;
    }

    const auto* indexTable = reinterpret_cast<const SampleEntry*>(basePtr + sizeof(ContainerHeader));

    int noteOnCount = 0, releaseCount = 0, pedalCount = 0, skippedCount = 0;

    for (uint32_t i = 0; i < header->numSampleEntries; ++i) {
        const auto& entry = indexTable[i];

        if (entry.fileOffset + entry.rawDataSize > (uint64_t)mappedFile->getSize()) {
            ++skippedCount;
            continue;
        }

        const float* rawFloatPtr = reinterpret_cast<const float*>(basePtr + entry.fileOffset);
        int numChannels = (int)entry.numChannels;
        int numSamples = (int)entry.totalNumSamples;
        double sampleRate = (double)entry.sampleRate;

        std::vector<const float*> tailChannelPointers((size_t)numChannels);
        for (int ch = 0; ch < numChannels; ++ch)
            tailChannelPointers[(size_t)ch] = rawFloatPtr + (ch * numSamples);

        int attackFrames = (entry.attackSampleSize > 0)
            ? (int)entry.attackSampleSize
            : juce::jmin(numSamples, juce::roundToInt(0.150 * sampleRate));

        juce::AudioBuffer<float> attackRamBuffer(numChannels, attackFrames);
        for (int ch = 0; ch < numChannels; ++ch) {
            attackRamBuffer.copyFrom(ch, 0, tailChannelPointers[(size_t)ch], attackFrames);
        }

        juce::SynthesiserSound::Ptr sound = new CustomSamplerSound(entry, attackRamBuffer, tailChannelPointers,
                                                                   numSamples, sampleRate, mappedFile);

        switch (entry.sampleType) {
            case SampleType::NoteOn:
                synthTarget.addNoteOnSoundToLayer(targetLayerIndex, sound);
                ++noteOnCount;
                break;
            case SampleType::ReleaseTrigger:
                synthTarget.addReleaseSoundToLayer(targetLayerIndex, sound);
                ++releaseCount;
                break;
            default:
                synthTarget.addPedalSoundToLayer(targetLayerIndex, sound);
                ++pedalCount;
                break;
        }
    }

    juce::Logger::writeToLog("SampleContainerReader: Loaded container '" + file.getFileName() + "' into layer " +
                             juce::String(targetLayerIndex) + " — " + juce::String(noteOnCount) + " NoteOn, " +
                             juce::String(releaseCount) + " Release, " + juce::String(pedalCount) + " Pedal sounds.");

    return true;
}
