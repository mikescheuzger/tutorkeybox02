#include "SamplePackager.h"
#include "SfzReader.h"
#include <algorithm>
#include <map>
#include <vector>

// =============================================================================
// Helper to parse note tokens (e.g. "A0", "C1", "D#3", "Eb4", "F#5")
// =============================================================================
static int parseNoteToken(const juce::String& token) {
    juce::String s = token.trim();
    if (s.isEmpty()) return 60;
    if (std::isdigit(s[0])) return s.getIntValue();

    int semitone = 0;
    int idx = 0;

    switch (std::toupper(s[idx])) {
        case 'C': semitone = 0; break;
        case 'D': semitone = 2; break;
        case 'E': semitone = 4; break;
        case 'F': semitone = 5; break;
        case 'G': semitone = 7; break;
        case 'A': semitone = 9; break;
        case 'B': semitone = 11; break;
        default: return 60;
    }
    ++idx;

    if (idx < s.length()) {
        if (s[idx] == '#' || s[idx] == 's' || s[idx] == 'S') { semitone += 1; ++idx; }
        else if (s[idx] == 'b' && idx + 1 < s.length() && (std::isdigit(s[idx + 1]) || s[idx + 1] == '-')) { semitone -= 1; ++idx; }
    }

    juce::String octStr;
    while (idx < s.length() && (std::isdigit(s[idx]) || s[idx] == '-'))
        octStr += s[idx++];

    int octave = octStr.isNotEmpty() ? octStr.getIntValue() : 4;
    return juce::jlimit(0, 127, (octave + 1) * 12 + semitone);
}

// =============================================================================
// Zero crossing search for attack buffer split
// =============================================================================
static int findZeroCrossing(const juce::AudioBuffer<float>& buf, int targetFrame) {
    int total = buf.getNumSamples();
    if (targetFrame >= total - 1) return total;
    const float* ch0 = buf.getReadPointer(0);
    int window = juce::jmin(1000, total - targetFrame - 2);
    for (int offset = 0; offset < window; ++offset) {
        int i = targetFrame + offset;
        if (ch0[i] * ch0[i + 1] <= 0.0f) return i;
    }
    return targetFrame;
}

// =============================================================================
// Universal Filename Parser (Salamander, TKB2, Single-Sample Pads)
// =============================================================================
struct ParsedWav {
    juce::File    file;
    SampleType    sampleType{ SampleType::NoteOn };
    VelocityLayer velocityLayer{ VelocityLayer::P };
    PedalState    pedalState{ PedalState::NoPedal };
    int           rootNote{ 60 };
    int           roundRobinIndex{ 0 };
    uint8_t       velLow{ 0 };
    uint8_t       velHigh{ 127 };
    juce::String  name;
};

static bool parseAnyFilename(const juce::File& audioFile, ParsedWav& out) {
    juce::String stem = audioFile.getFileNameWithoutExtension();
    out.file = audioFile;
    out.name = stem;

    // 1. Check Release Trigger patterns (e.g. rel1.flac to rel88.flac or Neumann_RT)
    if (stem.startsWithIgnoreCase("rel")) {
        out.sampleType = SampleType::ReleaseTrigger;
        int relNum = stem.substring(3).getIntValue();
        out.rootNote = (relNum > 0) ? juce::jlimit(21, 108, relNum + 20) : 60;
        return true;
    }

    // 2. Check Pedal Noise patterns (e.g. pedalD1.flac, pedalU1.flac)
    if (stem.startsWithIgnoreCase("pedalD")) {
        out.sampleType = SampleType::PedalDown;
        return true;
    }
    if (stem.startsWithIgnoreCase("pedalU")) {
        out.sampleType = SampleType::PedalUp;
        return true;
    }

    // 3. Check Salamander format (e.g. A0v1.flac to A0v16.flac or D#3v10.flac)
    int vIdx = stem.lastIndexOfIgnoreCase("v");
    if (vIdx > 0) {
        juce::String notePart = stem.substring(0, vIdx);
        juce::String velPart  = stem.substring(vIdx + 1);

        if (std::isdigit(notePart[0]) || std::isalpha(notePart[0])) {
            out.rootNote = parseNoteToken(notePart);
            int vNum = velPart.getIntValue(); // 1 to 16
            if (vNum >= 1 && vNum <= 16) {
                out.velLow  = (uint8_t)((vNum - 1) * 8);
                out.velHigh = (uint8_t)(vNum == 16 ? 127 : (vNum * 8 - 1));
                out.velocityLayer = (vNum <= 5) ? VelocityLayer::P : (vNum <= 11 ? VelocityLayer::MF : VelocityLayer::F);
                out.sampleType = SampleType::NoteOn;
                return true;
            }
        }
    }

    // 4. Fallback: Parse single note (e.g. WarmPad_C3.wav or AmbientPad.wav)
    out.sampleType = SampleType::NoteOn;
    out.rootNote = parseNoteToken(stem);
    out.velLow = 0;
    out.velHigh = 127;
    return true;
}

// =============================================================================
// SamplePackager::createPackage
// =============================================================================
bool SamplePackager::createPackage(const juce::File& inputWavDir, const juce::File& outputBinFile) {
    juce::Array<juce::File> audioFiles;

    // Check if input is an .sfz file
    if (inputWavDir.existsAsFile() && inputWavDir.getFileExtension().equalsIgnoreCase(".sfz")) {
        auto sfzRegions = SfzReader::parseSfzRegions(inputWavDir);
        juce::File baseDir = inputWavDir.getParentDirectory();

        for (const auto& r : sfzRegions) {
            juce::File f = baseDir.getChildFile(r.samplePath);
            if (!f.existsAsFile()) f = baseDir.getChildFile("Samples").getChildFile(r.samplePath);
            if (!f.existsAsFile()) f = baseDir.getChildFile("Samples").getChildFile(juce::File(r.samplePath).getFileName());
            if (!f.existsAsFile()) f = baseDir.getChildFile(juce::File(r.samplePath).getFileName());
            if (f.existsAsFile()) audioFiles.add(f);
        }
    } else if (inputWavDir.isDirectory()) {
        audioFiles = inputWavDir.findChildFiles(juce::File::findFiles, true, "*.wav;*.flac");
    } else if (inputWavDir.existsAsFile()) {
        audioFiles.add(inputWavDir);
    }

    if (audioFiles.isEmpty()) {
        juce::Logger::writeToLog("SamplePackager: No valid .wav or .flac files found!");
        return false;
    }

    audioFiles.sort();

    juce::AudioFormatManager formatMgr;
    formatMgr.registerFormat(new juce::WavAudioFormat(), true);
    formatMgr.registerFormat(new juce::FlacAudioFormat(), true);

    std::vector<ParsedWav> parsed;
    for (const auto& f : audioFiles) {
        ParsedWav pw;
        if (parseAnyFilename(f, pw)) parsed.push_back(pw);
    }

    if (parsed.empty()) return false;

    // If single sample pad: set keyLow = 0, keyHigh = 127
    if (parsed.size() == 1) {
        parsed[0].velLow = 0;
        parsed[0].velHigh = 127;
    }

    // Build unique root notes list for pitch-down calculation
    std::vector<int> noteOnRoots;
    for (const auto& p : parsed) {
        if (p.sampleType == SampleType::NoteOn) {
            if (std::find(noteOnRoots.begin(), noteOnRoots.end(), p.rootNote) == noteOnRoots.end())
                noteOnRoots.push_back(p.rootNote);
        }
    }
    std::sort(noteOnRoots.begin(), noteOnRoots.end());

    uint64_t audioDataStart = sizeof(ContainerHeader) + (parsed.size() * sizeof(SampleEntry));

    outputBinFile.deleteFile();
    juce::FileOutputStream outStream(outputBinFile);
    if (!outStream.openedOk()) return false;

    ContainerHeader hdr{};
    hdr.magic[0] = TKB_Magic[0];
    hdr.magic[1] = TKB_Magic[1];
    hdr.magic[2] = TKB_Magic[2];
    hdr.magic[3] = TKB_Magic[3];
    hdr.version = 2;
    hdr.numSampleEntries = (uint32_t)parsed.size();
    outStream.write(&hdr, sizeof(ContainerHeader));

    struct AudioBlock {
        juce::MemoryBlock data;
        int numChannels;
        int numSamples;
        double sampleRate;
        int zeroCrossing;
    };

    std::vector<AudioBlock> audioBlocks;
    std::vector<uint64_t> fileOffsets(parsed.size());
    uint64_t cursor = audioDataStart;

    for (size_t i = 0; i < parsed.size(); ++i) {
        std::unique_ptr<juce::AudioFormatReader> reader(formatMgr.createReaderFor(parsed[i].file));

        AudioBlock ab{};
        if (reader != nullptr) {
            ab.numChannels = (int)reader->numChannels;
            ab.numSamples = (int)reader->lengthInSamples;
            ab.sampleRate = reader->sampleRate;

            juce::AudioBuffer<float> buf(ab.numChannels, ab.numSamples);
            reader->read(&buf, 0, ab.numSamples, 0, true, true);

            int target150 = juce::roundToInt(0.150 * ab.sampleRate);
            ab.zeroCrossing = findZeroCrossing(buf, target150);

            size_t rawBytes = (size_t)(ab.numChannels * ab.numSamples) * sizeof(float);
            ab.data.setSize(rawBytes, false);
            float* dest = reinterpret_cast<float*>(ab.data.getData());
            for (int ch = 0; ch < ab.numChannels; ++ch)
                std::memcpy(dest + ch * ab.numSamples, buf.getReadPointer(ch), (size_t)ab.numSamples * sizeof(float));
        }

        fileOffsets[i] = cursor;
        cursor += ab.data.getSize();
        audioBlocks.push_back(std::move(ab));
    }

    // Write index table
    for (size_t i = 0; i < parsed.size(); ++i) {
        const auto& p = parsed[i];
        const auto& ab = audioBlocks[i];

        SampleEntry e{};
        e.sampleID = (uint32_t)(i + 1);
        p.name.copyToUTF8(e.name, sizeof(e.name) - 1);
        e.rootNote = (uint8_t)p.rootNote;

        if (parsed.size() == 1) {
            e.keyLow = 0;
            e.keyHigh = 127;
        } else {
            auto it = std::find(noteOnRoots.begin(), noteOnRoots.end(), p.rootNote);
            if (it != noteOnRoots.end()) {
                size_t idx = (size_t)std::distance(noteOnRoots.begin(), it);
                e.keyHigh = (uint8_t)p.rootNote;
                e.keyLow  = (idx == 0) ? 0 : (uint8_t)(noteOnRoots[idx - 1] + 1);
            } else {
                e.keyLow = (uint8_t)p.rootNote;
                e.keyHigh = (uint8_t)p.rootNote;
            }
        }

        e.velZoneLow = p.velLow;
        e.velZoneHigh = p.velHigh;
        e.zoneGainMultiplier = 1.0f;
        e.sampleType = p.sampleType;
        e.velocityLayer = p.velocityLayer;
        e.pedalState = p.pedalState;
        e.roundRobinIndex = 0;
        e.roundRobinCount = 1;
        e.numChannels = (uint32_t)ab.numChannels;
        e.sampleRate = (uint32_t)ab.sampleRate;
        e.totalNumSamples = (uint32_t)ab.numSamples;
        e.attackSampleSize = (uint32_t)ab.zeroCrossing;
        e.rawDataSize = (uint64_t)ab.data.getSize();
        e.fileOffset = fileOffsets[i];

        outStream.write(&e, sizeof(SampleEntry));
    }

    // Write audio blocks
    for (const auto& ab : audioBlocks) {
        outStream.write(ab.data.getData(), ab.data.getSize());
    }

    juce::Logger::writeToLog("SamplePackager: Successfully compiled TKB2 container '" + outputBinFile.getFileName() + "' from " + juce::String(parsed.size()) + " audio files.");
    return true;
}