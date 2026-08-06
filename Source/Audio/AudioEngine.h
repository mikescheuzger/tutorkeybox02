#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "../Synth/FXChannel.h"
#include "../Synth/LayeredSynth.h"
#include "MasterChain.h"
#include <functional>

// Forward declaration
struct MidiState;

// =============================================================================
// AudioEngine — Core Audio Hardware & Processing Engine
//
// Responsibilities:
//   • Hardware Audio I/O Management (CoreAudio on macOS, ALSA on Pi)
//   • Live MIDI input hotplug scanning & event routing
//   • Bidirectional MIDI support (local controller + UDP forwarded MIDI)
//   • Orchestrates audio pipeline:
//       LayeredSynth (4 Layers) ──(AUX Send)──> FXChannel (Reverb + Delay)
//                                                   │
//                                              (Sum to Main)
//                                                   ↓
//                                              MasterChain (Compressor →
//                                              Limiter → Clipper → VU Meter)
//                                                   ↓
//                                              Audio Hardware Output
//   • Zero heap allocation on the audio thread
// =============================================================================
class AudioEngine : public juce::AudioIODeviceCallback,
                    public juce::MidiInputCallback,
                    public juce::ChangeBroadcaster {
public:
  explicit AudioEngine(MidiState &stateToUpdate);
  ~AudioEngine() override;

  bool initialize();
  void shutdown();
  bool setBufferSize(int newBufferSize);
  void refreshMidiInputs(); // Live MIDI Hotplug Scanner

  bool isAudioOK() const { return audioDeviceOK; }
  juce::String getActiveAudioDeviceName() const;

  // Callback invoked when local or UDP MIDI arrives
  std::function<void(const juce::MidiMessage &)> onMidiMessageReceived;

  juce::AudioDeviceManager &getDeviceManager() { return deviceManager; }
  LayeredSynth &getSynth() { return synth; }
  FXChannel &getFXChannel() { return fxChannel; }
  MasterChain &getMasterChain() { return masterChain; }
  MidiState &getMidiState() { return midiState; }

  float getCpuUsage() const { return (float)deviceManager.getCpuUsage(); }
  int getActiveVoiceCount() const { return synth.getNumActiveVoices(); }

  /** Post external MIDI message (e.g. forwarded from network UDP) into audio
   * queue. */
  void postExternalMidiMessage(const juce::MidiMessage &message);

  // ── juce::MidiInputCallback ───────────────────────────────────────────────
  void handleIncomingMidiMessage(juce::MidiInput *source,
                                 const juce::MidiMessage &message) override;

  // ── juce::AudioIODeviceCallback ───────────────────────────────────────────
  void audioDeviceIOCallbackWithContext(
      const float *const *inputChannelData, int numInputChannels,
      float *const *outputChannelData, int numOutputChannels, int numSamples,
      const juce::AudioIODeviceCallbackContext &context) override;

  void audioDeviceAboutToStart(juce::AudioIODevice *device) override;
  void audioDeviceStopped() override;

private:
  MidiState &midiState;
  juce::AudioDeviceManager deviceManager;

  // Audio Processing Components
  LayeredSynth synth;
  FXChannel fxChannel;
  MasterChain masterChain;

  bool audioDeviceOK{false};

  // Thread-safe MIDI input queue
  juce::MidiBuffer incomingMidiBuffer;
  juce::CriticalSection midiLock;

  // Pre-allocated AUX send buffer (Zero Heap Allocation)
  juce::AudioBuffer<float> auxBuffer;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine);
};
