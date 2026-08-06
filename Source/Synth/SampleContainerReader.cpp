#include "SampleContainerReader.h"
#include "LayeredSynth.h"

// Persistent storage vector declared in SfzReader.cpp
extern std::vector<std::shared_ptr<juce::AudioBuffer<float>>> g_sfzPersistentBuffers;

// =============================================================================
// Helper 1: Universal Note Name & MIDI Number Parser
// =============================================================================
static int parseNoteToMidi(const juce::String& token, bool& outFoundNote) {
    juce::String s = token.trim();
    outFoundNote = false;
    if (s.isEmpty()) return -1;

    // Check if 3-digit raw MIDI number (e.g. 060, 048, 108)
    if (s.containsOnly("0123456789")) {
        int val = s.getIntValue();
        if (val >= 0 && val <= 127) {
            outFoundNote = true;
            return val;
        }
    }

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
        default: return -1;
    }
    ++idx;

    if (idx < s.length()) {
        if (s[idx] == '#' || s[idx] == 's') { semitone += 1; ++idx; }
        else if (s[idx] == 'b') { semitone -= 1; ++idx; }
    }

    juce::String octStr;
    while (idx < s.length() && (std::isdigit(s[idx]) || s[idx] == '-'))
        octStr += s[idx++];

    if (octStr.isEmpty()) return -1; // Must have octave number to be a valid note name (e.g. C4, A#2)

    int octave = octStr.getIntValue();
    int midiNote = (octave + 1) * 12 + semitone;

    if (midiNote >= 0 && midiNote <= 127) {
        outFoundNote = true;
        return midiNote;
    }

    return -1;
}

// =============================================================================
// Helper 2: Raw Sample Directory Loader with Universal Smart Parser & Category Filter
// =============================================================================
static bool loadRawSampleDirectory(const juce::File& dir, LayeredSynth& synthTarget, int targetLayerIndex) {
    auto audioFiles = dir.findChildFiles(juce::File::findFiles, true, "*.wav;*.flac");
    if (audioFiles.isEmpty()) return false;

    juce::AudioFormatManager formatMgr;
    formatMgr.registerFormat(new juce::WavAudioFormat(), true);
    formatMgr.registerFormat(new juce::FlacAudioFormat(), true);

    int loadedCount = 0;
    int ignoredCount = 0;

    for (const auto& f : audioFiles) {
        juce::String name = f.getFileNameWithoutExtension();

        SampleType detectedType = SampleType::NoteOn;
        VelocityLayer detectedVelLayer = VelocityLayer::P;
        PedalState detectedPedalState = PedalState::NoPedal;

        int note = -1;
        bool foundNoteToken = false;
        bool foundPedalToken = false;
        bool foundReleaseToken = false;

        uint8_t velLow = 0;
        uint8_t velHigh = 127;

        juce::StringArray tokens;
        tokens.addTokens(name, "_- ", "\"");

        // ── Stage 1, 2, 3: Parse Tokens ──────────────────────────────────────────
        for (const auto& tok : tokens) {
            juce::String t = tok.trim();

            // 1. Pedal & Release Classifier
            if (t.containsIgnoreCase("pedal_down") || t.containsIgnoreCase("withpedal") || t.equalsIgnoreCase("pedaldown")) {
                detectedType = SampleType::PedalDown;
                detectedPedalState = PedalState::WithPedal;
                foundPedalToken = true;
            }
            else if (t.containsIgnoreCase("pedal_up") || t.containsIgnoreCase("nopedal") || t.equalsIgnoreCase("pedalup")) {
                detectedType = SampleType::PedalUp;
                detectedPedalState = PedalState::NoPedal;
                foundPedalToken = true;
            }
            else if (t.equalsIgnoreCase("rt") || t.equalsIgnoreCase("release") || t.containsIgnoreCase("releasetrigger")) {
                detectedType = SampleType::ReleaseTrigger;
                foundReleaseToken = true;
            }

            // 2. Velocity Layer Resolver
            if (t.equalsIgnoreCase("pp") || t.equalsIgnoreCase("p") || t.equalsIgnoreCase("soft") || t.equalsIgnoreCase("v1")) {
                detectedVelLayer = VelocityLayer::P;
                velLow = 0; velHigh = 42;
            }
            else if (t.equalsIgnoreCase("mp") || t.equalsIgnoreCase("mf") || t.equalsIgnoreCase("med") || t.equalsIgnoreCase("medium") || t.equalsIgnoreCase("v2")) {
                detectedVelLayer = VelocityLayer::MF;
                velLow = 43; velHigh = 85;
            }
            else if (t.equalsIgnoreCase("f") || t.equalsIgnoreCase("ff") || t.equalsIgnoreCase("fff") || t.equalsIgnoreCase("loud") || t.equalsIgnoreCase("v3")) {
                detectedVelLayer = VelocityLayer::F;
                velLow = 86; velHigh = 127;
            }

            // 3. Pitch & Note Parser
            bool okNote = false;
            int parsedMidi = parseNoteToMidi(t, okNote);
            if (okNote) {
                note = parsedMidi;
                foundNoteToken = true;
            }
        }

        // ── Stage 4: Strict Category Filter & Exclusion Engine ───────────────────
        if (name.containsIgnoreCase("harm") || name.containsIgnoreCase("resonance") || name.containsIgnoreCase("sympathetic")) {
            juce::Logger::writeToLog("SampleContainerReader: Excluded harmonic sample -> " + f.getFileName());
            ++ignoredCount;
            continue;
        }

        // If file contains NO note token, NO pedal token, and NO release token, IGNORE COMPLETELY!
        if (!foundNoteToken && !foundPedalToken && !foundReleaseToken) {
            juce::Logger::writeToLog("SampleContainerReader: Ignored non-category file -> " + f.getFileName());
            ++ignoredCount;
            continue;
        }

        // Default note to C4 (60) if pedal noise without note
        if (note == -1) {
            note = 60;
        }

        std::unique_ptr<juce::AudioFormatReader> reader(formatMgr.createReaderFor(f));
        if (reader == nullptr) continue;

        int numSamples = (int)reader->lengthInSamples;
        int numChannels = (int)reader->numChannels;
        double sampleRate = reader->sampleRate;

        // Allocate persistent heap audio buffer
        auto heapBuffer = std::make_shared<juce::AudioBuffer<float>>(numChannels, numSamples);
        reader->read(heapBuffer.get(), 0, numSamples, 0, true, true);
        g_sfzPersistentBuffers.push_back(heapBuffer);

        std::vector<const float*> channelPointers((size_t)numChannels);
        for (int ch = 0; ch < numChannels; ++ch)
            channelPointers[(size_t)ch] = heapBuffer->getReadPointer(ch);

        SampleEntry entry{};
        entry.sampleID = (uint32_t)(loadedCount + 1);
        name.copyToUTF8(entry.name, sizeof(entry.name) - 1);
        entry.rootNote = (uint8_t)note;
        entry.keyLow = (uint8_t)juce::jmax(0, note - 2);
        entry.keyHigh = (uint8_t)juce::jmin(127, note + 2);
        entry.velZoneLow = velLow;
        entry.velZoneHigh = velHigh;
        entry.zoneGainMultiplier = 1.0f;
        entry.sampleType = detectedType;
        entry.velocityLayer = detectedVelLayer;
        entry.pedalState = detectedPedalState;
        entry.numChannels = (uint32_t)numChannels;
        entry.sampleRate = (uint32_t)sampleRate;
        entry.totalNumSamples = (uint32_t)numSamples;
        entry.attackSampleSize = (uint32_t)numSamples;

        juce::SynthesiserSound::Ptr sound = new CustomSamplerSound(entry, *heapBuffer, channelPointers, numSamples, sampleRate, nullptr);

        switch (entry.sampleType) {
            case SampleType::NoteOn:
                synthTarget.addNoteOnSoundToLayer(targetLayerIndex, sound);
                break;
            case SampleType::ReleaseTrigger:
                synthTarget.addReleaseSoundToLayer(targetLayerIndex, sound);
                break;
            default:
                synthTarget.addPedalSoundToLayer(targetLayerIndex, sound);
                break;
        }

        ++loadedCount;
    }

    juce::Logger::writeToLog("SampleContainerReader: Loaded " + juce::String(loadedCount) + " valid samples from " + dir.getFileName() + " (" + juce::String(ignoredCount) + " non-category files ignored).");
    return loadedCount > 0;
}

// =============================================================================
// Main Container File Entrypoint
// =============================================================================
bool SampleContainerReader::loadContainerFile(const juce::File& file,
                                              LayeredSynth&     synthTarget,
                                              int               targetLayerIndex) {
    if (file.isDirectory()) {
        auto sfzFiles = file.findChildFiles(juce::File::findFiles, true, "*.sfz");
        if (!sfzFiles.isEmpty()) {
            return SfzReader::loadSfzFile(sfzFiles[0], synthTarget, targetLayerIndex);
        }
        return loadRawSampleDirectory(file, synthTarget, targetLayerIndex);
    }

    if (!file.existsAsFile()) {
        juce::Logger::writeToLog("SampleContainerReader Error: File does not exist — " + file.getFullPathName());
        return false;
    }

    // ── Check if file is an SFZ Descriptor File (.sfz) ────────────────────────
    if (file.getFileExtension().equalsIgnoreCase(".sfz")) {
        return SfzReader::loadSfzFile(file, synthTarget, targetLayerIndex);
    }

    // ── Check if file is a raw audio sample (.wav / .flac) ─────────────────────
    if (file.getFileExtension().equalsIgnoreCase(".wav") || file.getFileExtension().equalsIgnoreCase(".flac")) {
        return loadRawSampleDirectory(file.getParentDirectory(), synthTarget, targetLayerIndex);
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
