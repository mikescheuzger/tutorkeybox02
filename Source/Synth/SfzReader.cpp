#include "SfzReader.h"
#include "LayeredSynth.h"

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

std::vector<SfzRegion> SfzReader::parseSfzRegions(const juce::File& sfzFile) {
    std::vector<SfzRegion> regions;
    if (!sfzFile.existsAsFile()) return regions;

    juce::StringArray lines;
    lines.addLines(sfzFile.loadFileAsString());

    SfzRegion currentGroupDefaults;
    SfzRegion currentRegion;
    bool inRegion = false;

    for (auto line : lines) {
        line = line.trim();
        if (line.isEmpty() || line.startsWith("//")) continue;

        if (line.startsWithIgnoreCase("<group>")) {
            currentGroupDefaults = SfzRegion{};
            juce::String rest = line.substring(7);
            juce::StringArray tokens;
            tokens.addTokens(rest, " \t", "\"");

            for (const auto& tok : tokens) {
                if (tok.contains("=")) {
                    auto key = tok.upToFirstOccurrenceOf("=", false, true).trim();
                    auto val = tok.fromFirstOccurrenceOf("=", false, true).trim();

                    if (key.equalsIgnoreCase("lokey")) currentGroupDefaults.lokey = noteNameToMidi(val);
                    else if (key.equalsIgnoreCase("hikey")) currentGroupDefaults.hikey = noteNameToMidi(val);
                    else if (key.equalsIgnoreCase("pitch_keycenter")) currentGroupDefaults.pitchKeycenter = noteNameToMidi(val);
                    else if (key.equalsIgnoreCase("lovel")) currentGroupDefaults.lovel = val.getIntValue();
                    else if (key.equalsIgnoreCase("hivel")) currentGroupDefaults.hivel = val.getIntValue();
                    else if (key.equalsIgnoreCase("trigger") && val.equalsIgnoreCase("release"))
                        currentGroupDefaults.sampleType = SampleType::ReleaseTrigger;
                }
            }
        }
        else if (line.startsWithIgnoreCase("<region>")) {
            if (inRegion && currentRegion.samplePath.isNotEmpty()) {
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

                    if (key.equalsIgnoreCase("sample")) currentRegion.samplePath = val;
                    else if (key.equalsIgnoreCase("lokey")) currentRegion.lokey = noteNameToMidi(val);
                    else if (key.equalsIgnoreCase("hikey")) currentRegion.hikey = noteNameToMidi(val);
                    else if (key.equalsIgnoreCase("key")) {
                        int k = noteNameToMidi(val);
                        currentRegion.lokey = k;
                        currentRegion.hikey = k;
                        currentRegion.pitchKeycenter = k;
                    }
                    else if (key.equalsIgnoreCase("pitch_keycenter")) currentRegion.pitchKeycenter = noteNameToMidi(val);
                    else if (key.equalsIgnoreCase("lovel")) currentRegion.lovel = val.getIntValue();
                    else if (key.equalsIgnoreCase("hivel")) currentRegion.hivel = val.getIntValue();
                    else if (key.equalsIgnoreCase("trigger") && val.equalsIgnoreCase("release"))
                        currentRegion.sampleType = SampleType::ReleaseTrigger;
                }
            }
        }
    }

    if (inRegion && currentRegion.samplePath.isNotEmpty()) {
        regions.push_back(currentRegion);
    }

    return regions;
}

bool SfzReader::loadSfzFile(const juce::File& sfzFile, LayeredSynth& synthTarget, int targetLayerIndex) {
    if (!sfzFile.existsAsFile()) return false;

    auto regions = parseSfzRegions(sfzFile);
    if (regions.empty()) return false;

    juce::File baseDir = sfzFile.getParentDirectory();
    juce::AudioFormatManager formatMgr;
    formatMgr.registerFormat(new juce::WavAudioFormat(), true);
    formatMgr.registerFormat(new juce::FlacAudioFormat(), true);

    int loadedCount = 0;

    for (const auto& r : regions) {
        juce::File audioFile = baseDir.getChildFile(r.samplePath);
        if (!audioFile.existsAsFile()) {
            audioFile = baseDir.getChildFile("Samples").getChildFile(r.samplePath);
        }
        if (!audioFile.existsAsFile()) continue;

        std::unique_ptr<juce::AudioFormatReader> reader(formatMgr.createReaderFor(audioFile));
        if (reader == nullptr) continue;

        int numSamples = (int)reader->lengthInSamples;
        int numChannels = (int)reader->numChannels;
        double sampleRate = reader->sampleRate;

        // Read audio into RAM buffer
        juce::AudioBuffer<float> buffer(numChannels, numSamples);
        reader->read(&buffer, 0, numSamples, 0, true, true);

        // Build channel pointers vector
        std::vector<const float*> channelPointers((size_t)numChannels);
        for (int ch = 0; ch < numChannels; ++ch)
            channelPointers[(size_t)ch] = buffer.getReadPointer(ch);

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
        entry.sampleType = r.sampleType;
        entry.velocityLayer = VelocityLayer::P;
        entry.pedalState = PedalState::NoPedal;
        entry.numChannels = (uint32_t)numChannels;
        entry.sampleRate = (uint32_t)sampleRate;
        entry.totalNumSamples = (uint32_t)numSamples;
        entry.attackSampleSize = (uint32_t)numSamples;

        juce::SynthesiserSound::Ptr sound = new CustomSamplerSound(entry, buffer, channelPointers, numSamples, sampleRate, nullptr);

        switch (r.sampleType) {
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
