#include "SfzReader.h"
#include "LayeredSynth.h"
#include <map>

// Helper to convert note names like C4, D#3, Eb2 to MIDI number
static int noteNameToMidi(const juce::String& name) {
    juce::String s = name.trim();
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
        if (s[idx] == '#' || s[idx] == 's') { semitone += 1; ++idx; }
        else if (s[idx] == 'b') { semitone -= 1; ++idx; }
    }

    juce::String octStr;
    while (idx < s.length() && (std::isdigit(s[idx]) || s[idx] == '-'))
        octStr += s[idx++];

    int octave = octStr.isNotEmpty() ? octStr.getIntValue() : 4;
    return juce::jlimit(0, 127, (octave + 1) * 12 + semitone);
}

// Recursive parser for SFZ files resolving #include and #define directives
static void readSfzFileRecursive(const juce::File& file, const juce::File& rootDir, juce::StringArray& outLines, std::map<juce::String, juce::String>& defines) {
    if (!file.existsAsFile()) return;

    juce::String fileText = file.loadFileAsString();
    juce::StringArray rawLines;
    rawLines.addLines(fileText);

    juce::File parentDir = file.getParentDirectory();

    for (auto line : rawLines) {
        line = line.trim();
        if (line.isEmpty() || line.startsWith("//")) continue;

        // 1. Process #define $MACRO value
        if (line.startsWithIgnoreCase("#define")) {
            juce::String rest = line.substring(7).trim();
            auto key = rest.upToFirstOccurrenceOf(" ", false, true).trim();
            auto val = rest.fromFirstOccurrenceOf(" ", false, true).trim();
            if (key.isNotEmpty()) {
                defines[key] = val;
            }
            continue;
        }

        // 2. Process #include "subfile.txt"
        if (line.containsIgnoreCase("#include")) {
            juce::StringArray tokens;
            tokens.addTokens(line, " \t", "\"");
            juce::String lineClean = "";
            juce::StringArray incFilesToRead;

            for (const auto& tok : tokens) {
                juce::String cleanTok = tok.unquoted().trim();
                if (cleanTok.startsWithIgnoreCase("#include")) continue;

                if (cleanTok.endsWithIgnoreCase(".txt") || cleanTok.endsWithIgnoreCase(".sfz")) {
                    incFilesToRead.add(cleanTok);
                } else {
                    lineClean += tok + " ";
                }
            }

            lineClean = lineClean.trim();

            // Substitute defines in group line if present
            for (const auto& kv : defines) {
                lineClean = lineClean.replace(kv.first, kv.second);
            }

            if (lineClean.isNotEmpty()) {
                outLines.add(lineClean);
            }

            // Recursively expand includes AFTER group parameters are emitted!
            for (const auto& incPath : incFilesToRead) {
                juce::File incFile = rootDir.getChildFile(incPath);
                if (!incFile.existsAsFile()) {
                    incFile = parentDir.getChildFile(incPath);
                }
                if (!incFile.existsAsFile()) {
                    incFile = rootDir.getChildFile("Data").getChildFile(juce::File(incPath).getFileName());
                }
                if (incFile.existsAsFile()) {
                    readSfzFileRecursive(incFile, rootDir, outLines, defines);
                }
            }

            continue;
        }

        // 3. Substitute defines
        for (const auto& kv : defines) {
            line = line.replace(kv.first, kv.second);
        }

        if (line.isNotEmpty()) {
            outLines.add(line);
        }
    }
}

std::vector<SfzRegion> SfzReader::parseSfzRegions(const juce::File& sfzFile) {
    std::vector<SfzRegion> regions;
    if (!sfzFile.existsAsFile()) return regions;

    juce::StringArray lines;
    std::map<juce::String, juce::String> defines;
    juce::File rootDir = sfzFile.getParentDirectory();
    readSfzFileRecursive(sfzFile, rootDir, lines, defines);

    SfzRegion currentGroupDefaults;
    SfzRegion currentRegion;
    bool inRegion = false;
    juce::String defaultPath = "";

    for (auto line : lines) {
        line = line.trim();
        if (line.isEmpty() || line.startsWith("//")) continue;

        if (line.startsWithIgnoreCase("default_path=")) {
            defaultPath = line.fromFirstOccurrenceOf("default_path=", false, true).trim();
        }
        else if (line.startsWithIgnoreCase("<group>")) {
            currentGroupDefaults = SfzRegion{};
            juce::String rest = line.substring(7);
            juce::StringArray tokens;
            tokens.addTokens(rest, " \t", "\"");

            for (const auto& tok : tokens) {
                if (tok.contains("=")) {
                    auto key = tok.upToFirstOccurrenceOf("=", false, true).trim();
                    auto val = tok.fromFirstOccurrenceOf("=", false, true).trim();

                    if (key == "lokey") currentGroupDefaults.lokey = noteNameToMidi(val);
                    else if (key == "hikey") currentGroupDefaults.hikey = noteNameToMidi(val);
                    else if (key == "pitch_keycenter") currentGroupDefaults.pitchKeycenter = noteNameToMidi(val);
                    else if (key == "lovel") currentGroupDefaults.lovel = val.getIntValue();
                    else if (key == "hivel") currentGroupDefaults.hivel = val.getIntValue();
                }
            }
        }
        else if (line.startsWithIgnoreCase("<region>")) {
            if (inRegion && currentRegion.samplePath.isNotEmpty()) {
                if (defaultPath.isNotEmpty() && !currentRegion.samplePath.startsWithIgnoreCase(defaultPath)) {
                    currentRegion.samplePath = defaultPath + currentRegion.samplePath;
                }
                regions.push_back(currentRegion);
            }
            currentRegion = currentGroupDefaults;
            inRegion = true;

            juce::String rest = line.substring(8);
            juce::StringArray tokens;
            tokens.addTokens(rest, " \t", "\"");

            for (const auto& tok : tokens) {
                if (tok.contains("=")) {
                    auto key = tok.upToFirstOccurrenceOf("=", false, true).trim();
                    auto val = tok.fromFirstOccurrenceOf("=", false, true).trim();

                    if (key == "sample") currentRegion.samplePath = val;
                    else if (key == "lokey") currentRegion.lokey = noteNameToMidi(val);
                    else if (key == "hikey") currentRegion.hikey = noteNameToMidi(val);
                    else if (key == "key") {
                        int k = noteNameToMidi(val);
                        currentRegion.lokey = k;
                        currentRegion.hikey = k;
                        currentRegion.pitchKeycenter = k;
                    }
                    else if (key == "pitch_keycenter") currentRegion.pitchKeycenter = noteNameToMidi(val);
                    else if (key == "lovel") currentRegion.lovel = val.getIntValue();
                    else if (key == "hivel") currentRegion.hivel = val.getIntValue();
                }
            }
        }
    }

    if (inRegion && currentRegion.samplePath.isNotEmpty()) {
        if (defaultPath.isNotEmpty() && !currentRegion.samplePath.startsWithIgnoreCase(defaultPath)) {
            currentRegion.samplePath = defaultPath + currentRegion.samplePath;
        }
        regions.push_back(currentRegion);
    }

    return regions;
}

// Global persistent storage of audio buffers loaded from SFZ/WAV files to prevent dangling pointers
std::vector<std::shared_ptr<juce::AudioBuffer<float>>> g_sfzPersistentBuffers;

bool SfzReader::loadSfzFile(const juce::File& sfzFile, LayeredSynth& synthTarget, int targetLayerIndex) {
    if (!sfzFile.existsAsFile()) return false;

    auto regions = parseSfzRegions(sfzFile);
    if (regions.empty()) {
        juce::Logger::writeToLog("SfzReader Error: No regions found in " + sfzFile.getFullPathName());
        return false;
    }

    juce::File baseDir = sfzFile.getParentDirectory();
    juce::AudioFormatManager formatMgr;
    formatMgr.registerFormat(new juce::WavAudioFormat(), true);
    formatMgr.registerFormat(new juce::FlacAudioFormat(), true);

    int loadedCount = 0;

    for (const auto& r : regions) {
        if (r.samplePath.containsIgnoreCase("harmL") || r.samplePath.containsIgnoreCase("harmS") ||
            r.samplePath.containsIgnoreCase("harm") || r.samplePath.containsIgnoreCase("resonance") ||
            r.samplePath.containsIgnoreCase("sympathetic")) {
            juce::Logger::writeToLog("SfzReader: Excluded harmonic resonance layer -> " + r.samplePath);
            continue;
        }

        juce::File audioFile = baseDir.getChildFile(r.samplePath);
        if (!audioFile.existsAsFile()) {
            audioFile = baseDir.getChildFile("Samples").getChildFile(r.samplePath);
        }

        std::unique_ptr<juce::AudioFormatReader> reader(formatMgr.createReaderFor(audioFile));
        if (reader == nullptr) continue;

        int numSamples = (int)reader->lengthInSamples;
        int numChannels = (int)reader->numChannels;
        double sampleRate = reader->sampleRate;

        // Allocate persistent heap audio buffer
        auto heapBuffer = std::make_shared<juce::AudioBuffer<float>>(numChannels, numSamples);
        reader->read(heapBuffer.get(), 0, numSamples, 0, true, true);
        g_sfzPersistentBuffers.push_back(heapBuffer);

        // Build persistent channel pointers
        std::vector<const float*> channelPointers((size_t)numChannels);
        for (int ch = 0; ch < numChannels; ++ch)
            channelPointers[(size_t)ch] = heapBuffer->getReadPointer(ch);

        // Build SampleEntry metadata
        SampleEntry entry{};
        entry.sampleID = (uint32_t)(loadedCount + 1);
        audioFile.getFileNameWithoutExtension().copyToUTF8(entry.name, sizeof(entry.name) - 1);
        entry.rootNote = (uint8_t)r.pitchKeycenter;
        entry.keyLow = (uint8_t)r.lokey;
        entry.keyHigh = (uint8_t)r.hikey;
        entry.velZoneLow = (uint8_t)r.lovel;
        entry.velZoneHigh = (uint8_t)r.hivel;
        entry.zoneGainMultiplier = 1.0f;
        juce::String stem = audioFile.getFileNameWithoutExtension();
        if (stem.startsWithIgnoreCase("pedalD") || stem.startsWithIgnoreCase("pedal_down")) {
            entry.sampleType = SampleType::PedalDown;
        } else if (stem.startsWithIgnoreCase("pedalU") || stem.startsWithIgnoreCase("pedal_up")) {
            entry.sampleType = SampleType::PedalUp;
        } else if (stem.startsWithIgnoreCase("rel") || stem.startsWithIgnoreCase("release")) {
            entry.sampleType = SampleType::ReleaseTrigger;
        } else {
            entry.sampleType = SampleType::NoteOn;
        }

        entry.velocityLayer = VelocityLayer::P;
        entry.pedalState = PedalState::NoPedal;
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

    juce::Logger::writeToLog("SfzReader Success: Loaded " + juce::String(loadedCount) + " regions from SFZ: " + sfzFile.getFileName());
    return loadedCount > 0;
}
