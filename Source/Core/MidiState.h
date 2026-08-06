#pragma once
#include <juce_events/juce_events.h>
#include <juce_core/juce_core.h>
#include <atomic>
#include <cstring>

/**
 * Thread-safe atomic model for MIDI state and live sample telemetry inspector.
 * Inherits juce::ChangeBroadcaster to notify GUI windows on sample triggers.
 */
struct MidiState : public juce::ChangeBroadcaster {
    std::atomic<bool> isNoteActive{ false };
    std::atomic<int> currentNote{ -1 };
    std::atomic<float> currentVelocity{ 0.0f };
    std::atomic<bool> isPhysicalKeyDown{ false };
    std::atomic<bool> isSustainPedalDown{ false };

    // Live Sample Mapping Inspector Telemetry
    char lastSampleName[64]{ "None" };
    std::atomic<int> lastRootNote{ -1 };
    std::atomic<int> lastKeyLow{ 0 };
    std::atomic<int> lastKeyHigh{ 127 };
    std::atomic<int> lastVelLow{ 0 };
    std::atomic<int> lastVelHigh{ 127 };

    void noteOn(int noteNumber, float velocity) noexcept {
        currentNote.store(noteNumber, std::memory_order_relaxed);
        currentVelocity.store(velocity, std::memory_order_relaxed);
        isNoteActive.store(true, std::memory_order_relaxed);
        isPhysicalKeyDown.store(true, std::memory_order_relaxed);
    }

    void noteOff(int noteNumber) noexcept {
        if (currentNote.load(std::memory_order_relaxed) == noteNumber) {
            isPhysicalKeyDown.store(false, std::memory_order_relaxed);
            if (!isSustainPedalDown.load(std::memory_order_relaxed)) {
                isNoteActive.store(false, std::memory_order_relaxed);
                currentVelocity.store(0.0f, std::memory_order_relaxed);
            }
        }
    }

    void setSustainPedal(bool isDown) noexcept {
        isSustainPedalDown.store(isDown, std::memory_order_relaxed);
        if (!isDown && !isPhysicalKeyDown.load(std::memory_order_relaxed)) {
            isNoteActive.store(false, std::memory_order_relaxed);
            currentVelocity.store(0.0f, std::memory_order_relaxed);
        }
    }

    void updateSampleInspector(const char* name, int root, int kLow, int kHigh,
                               int vLow, int vHigh) noexcept {
        if (name != nullptr) {
            std::strncpy(lastSampleName, name, sizeof(lastSampleName) - 1);
            lastSampleName[sizeof(lastSampleName) - 1] = '\0';
        }
        lastRootNote.store(root, std::memory_order_relaxed);
        lastKeyLow.store(kLow, std::memory_order_relaxed);
        lastKeyHigh.store(kHigh, std::memory_order_relaxed);
        lastVelLow.store(vLow, std::memory_order_relaxed);
        lastVelHigh.store(vHigh, std::memory_order_relaxed);

        sendChangeMessage();
    }

    // Getter helper methods for GUI listeners
    juce::String getLastSampleName() const { return juce::String(lastSampleName); }
    int getLastSampleRoot() const          { return lastRootNote.load(); }
    int getLastSampleKeyLow() const        { return lastKeyLow.load(); }
    int getLastSampleKeyHigh() const       { return lastKeyHigh.load(); }
    int getLastSampleVelLow() const        { return lastVelLow.load(); }
    int getLastSampleVelHigh() const       { return lastVelHigh.load(); }

    void reset() noexcept {
        isNoteActive.store(false, std::memory_order_relaxed);
        currentNote.store(-1, std::memory_order_relaxed);
        currentVelocity.store(0.0f, std::memory_order_relaxed);
        isPhysicalKeyDown.store(false, std::memory_order_relaxed);
        isSustainPedalDown.store(false, std::memory_order_relaxed);
        lastRootNote.store(-1, std::memory_order_relaxed);
        std::strncpy(lastSampleName, "None", sizeof(lastSampleName));
    }
};
