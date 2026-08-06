#pragma once
#include "CustomSamplerSound.h"
#include "SampleHeader.h"
#include "SfzReader.h"
#include <juce_audio_formats/juce_audio_formats.h>

class LayeredSynth;

/**
 * SampleContainerReader — Static Utility to parse .bin containers AND .sfz files
 */
class SampleContainerReader {
public:
    SampleContainerReader() = delete; // Static utility class

    /** Loads sample container (.bin) OR instrument descriptor (.sfz) into synthTarget. */
    static bool loadContainerFile(const juce::File& file,
                                  LayeredSynth&     synthTarget,
                                  int               targetLayerIndex);
};
