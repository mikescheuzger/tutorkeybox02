#pragma once
#include <cstdint>

#pragma pack(push, 1)  // Packed binary struct — no padding bytes

// ─────────────────────────────────────────────────────────────────────────────
// Magic identifier at byte 0 of every .bin container
// ─────────────────────────────────────────────────────────────────────────────
constexpr char TKB_Magic[4] = {'T', 'K', 'B', '2'};  // '2' = TutorKeyBox02 format

// ─────────────────────────────────────────────────────────────────────────────
// Container File Header — sits at byte 0 of the .bin file
// ─────────────────────────────────────────────────────────────────────────────
struct ContainerHeader {
    char     magic[4];           // Must match TKB_Magic = "TKB2"
    uint32_t version;            // Format version (currently 2)
    uint32_t numSampleEntries;   // Total number of SampleEntry records in the index
};

// ─────────────────────────────────────────────────────────────────────────────
// SampleType — what kind of event triggers this sample
// ─────────────────────────────────────────────────────────────────────────────
enum class SampleType : uint8_t {
    NoteOn         = 0,  // Normal note — triggered on key press
    ReleaseTrigger = 1,  // Damper drop sound — triggered on key release
    PedalDown      = 2,  // Sustain pedal press click (CC64 crosses threshold going down)
    PedalUp        = 3   // Sustain pedal release click (CC64 crosses threshold going up)
};

// ─────────────────────────────────────────────────────────────────────────────
// VelocityLayer — which of the 3 recorded dynamic layers this sample belongs to
// ─────────────────────────────────────────────────────────────────────────────
enum class VelocityLayer : uint8_t {
    P  = 0,  // Piano  — soft
    MF = 1,  // Mezzo-Forte — medium
    F  = 2   // Forte — loud
};

// ─────────────────────────────────────────────────────────────────────────────
// PedalState — sustain pedal position during the recording session
// ─────────────────────────────────────────────────────────────────────────────
enum class PedalState : uint8_t {
    NoPedal   = 0,  // Recorded without sustain pedal (dry, damped string)
    WithPedal = 1   // Recorded with sustain pedal held (open, resonant string)
};

// ─────────────────────────────────────────────────────────────────────────────
// SampleEntry — one record in the binary index for each individual sample
// ─────────────────────────────────────────────────────────────────────────────
struct SampleEntry {
    uint32_t      sampleID;           // Unique ID — assigned sequentially by the Packager

    char          name[64];           // Human-readable label
                                      // e.g. "Neumann M49_MF_A3_rr2_withpedal"

    // ── Pitch & Key Zone ──────────────────────────────────────────────────────
    uint8_t       rootNote;           // MIDI note of the actual recording (pitch reference)
    uint8_t       keyLow;             // Lowest MIDI note this sample covers (inclusive)
    uint8_t       keyHigh;            // Highest MIDI note this sample covers (inclusive)

    // ── Velocity Zone (9-zone system) ────────────────────────────────────────
    uint8_t       velZoneLow;         // Lowest MIDI velocity this entry responds to
    uint8_t       velZoneHigh;        // Highest MIDI velocity this entry responds to
    float         zoneGainMultiplier; // dB-correct gain baked by Packager:
                                      //   0.501 = -6 dB (soft sub-zone)
                                      //   0.708 = -3 dB (medium sub-zone)
                                      //   1.000 =  0 dB (full level sub-zone)

    // ── Sample Classification ─────────────────────────────────────────────────
    SampleType    sampleType;         // NoteOn / ReleaseTrigger / PedalDown / PedalUp
    VelocityLayer velocityLayer;      // P / MF / F (which recorded dynamic layer)
    PedalState    pedalState;         // NoPedal / WithPedal (recording condition)

    // ── Round-Robin ───────────────────────────────────────────────────────────
    uint8_t       roundRobinIndex;    // Which take this is (0-based: 0, 1, 2 ...)
    uint8_t       roundRobinCount;    // Total takes for this note + layer + pedalState combo

    // ── Audio Format ──────────────────────────────────────────────────────────
    uint32_t      numChannels;        // 1 = mono, 2 = stereo
    uint32_t      sampleRate;         // Recording sample rate in Hz (e.g. 44100, 48000)
    uint32_t      totalNumSamples;    // Total audio frames in the float array
    uint32_t      attackSampleSize;   // Frames pre-loaded into RAM (approx. first 150 ms)
    uint64_t      rawDataSize;        // Byte size = totalNumSamples × numChannels × sizeof(float)

    // ── File Location ─────────────────────────────────────────────────────────
    uint64_t      fileOffset;         // Byte offset from start of .bin to this sample's audio data
};

#pragma pack(pop)
