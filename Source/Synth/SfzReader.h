#pragma once
#include "CustomSamplerSound.h"
#include "SampleHeader.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>

// Forward declaration
class LayeredSynth;

/**
 * SfzRegion — Parsed region entry from an SFZ descriptor file
 */
struct SfzRegion {
    juce::String samplePath;
    int          lokey{ 0 };
    int          hikey{ 127 };
    int          pitchKeycenter{ 60 };
    int          lovel{ 0 };
    int          hivel{ 127 };
    SampleType   sampleType{ SampleType::NoteOn };
};

/**
 * SfzReader — Native SFZ File Parser & Sample Loader Utility
 *
 * Responsibilities:
 *   • Parses standard .sfz text files (<region> tags, sample paths, key/vel ranges)
 *   • Decodes WAV and FLAC audio files (using FlacAudioFormat / WavAudioFormat)
 *   • Registers sounds directly into LayeredSynth
 */
class SfzReader {
public:
    SfzReader() = delete; // Static utility class

    /** Parses sfzFile and loads audio samples directly into targetLayerIndex of synthTarget. */
    static bool loadSfzFile(const juce::File& sfzFile,
                            LayeredSynth&     synthTarget,
                            int               targetLayerIndex);

    /** Parses sfzFile into a vector of SfzRegion descriptors. */
    static std::vector<SfzRegion> parseSfzRegions(const juce::File& sfzFile);
};
