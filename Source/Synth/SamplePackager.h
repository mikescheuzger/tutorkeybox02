#pragma once
#include "SampleHeader.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>

/**
 * SamplePackager — static utility that converts a folder of WAV files into
 * a monolithic TKB2 .bin container for use with SampleContainerReader.
 *
 * Supported filename convention:
 *   NoteOn:        [Instrument]_[P|MF|F]_[NOTE][OCT]_rr[N]_[nopedal|withpedal].wav
 *   ReleaseTrigger:[Instrument]_RT_[NOTE][OCT]_rr[N]_rt.wav
 *   PedalDown:     [Instrument]_PEDAL_DOWN_[NOTE][OCT]_rr[N]_pedal_down.wav
 *   PedalUp:       [Instrument]_PEDAL_UP_[NOTE][OCT]_rr[N]_pedal_up.wav
 *
 * Velocity zone mapping (9 zones, dB-correct gain steps):
 *   P  layer → zones 1–3 (vel   1–42), gains 0.501 / 0.708 / 1.000
 *   MF layer → zones 4–6 (vel  43–84), gains 0.501 / 0.708 / 1.000
 *   F  layer → zones 7–9 (vel 85–127), gains 0.501 / 0.708 / 1.000
 *
 * Pitch interpolation: ALWAYS down from the nearest recorded note above.
 *   keyHigh = rootNote, keyLow = previousRoot + 1.
 */
class SamplePackager {
public:
    SamplePackager() = delete;  // Static utility — not instantiable

    /**
     * Scans inputWavDir for .wav files, parses the TKB2 filename convention,
     * and writes a packed .bin container to outputBinFile.
     *
     * @param inputWavDir    Directory containing .wav sample files.
     * @param outputBinFile  Target .bin container path to write.
     * @return true on success, false if no valid files found or write failed.
     */
    static bool createPackage(const juce::File& inputWavDir,
                              const juce::File& outputBinFile);
};
