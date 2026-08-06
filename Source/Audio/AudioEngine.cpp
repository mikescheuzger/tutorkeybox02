#include "AudioEngine.h"
#include "../Core/MidiState.h"

// =============================================================================
// Constructor — Initialises processing components with MidiState reference
// =============================================================================
AudioEngine::AudioEngine(MidiState &stateToUpdate)
    : midiState(stateToUpdate), synth(stateToUpdate) {}

AudioEngine::~AudioEngine() { shutdown(); }

void AudioEngine::addMidiMessageListener(std::function<void(const juce::MidiMessage &)> listener) {
  if (listener != nullptr) {
    midiListeners.push_back(listener);
  }
}

// =============================================================================
// initialize — Scans audio devices, selects best hardware output (iO|2, ALSA,
// CoreAudio)
// =============================================================================
bool AudioEngine::initialize() {
  audioDeviceOK = false;

#if JUCE_MAC
  juce::String preferredDeviceType = "CoreAudio";
#else
  juce::String preferredDeviceType = "ALSA";
#endif

  const auto &availableTypes = deviceManager.getAvailableDeviceTypes();

  for (auto *type : availableTypes) {
    if (type->getTypeName().equalsIgnoreCase(preferredDeviceType)) {
      deviceManager.setCurrentAudioDeviceType(preferredDeviceType, true);
      break;
    }
  }

  juce::String bestOutputDevice = "";
  auto *currentType = deviceManager.getCurrentDeviceTypeObject();

  if (currentType != nullptr) {
    currentType->scanForDevices();
    auto outputDevices = currentType->getDeviceNames(false); // output devices
    int highestScore = -1;

    for (const auto &devName : outputDevices) {
      int score = 1;
      if (devName.containsIgnoreCase("hw:CARD=iO2") ||
          devName.containsIgnoreCase("plughw:CARD=iO2") ||
          devName.containsIgnoreCase("iO2") ||
          devName.containsIgnoreCase("iO|2")) {
        score = 10; // Top priority: Direct hardware Alesis iO|2 interface!
      } else if (devName.startsWithIgnoreCase("hw:") ||
                 devName.startsWithIgnoreCase("plughw:")) {
        score = 5; // Direct ALSA Hardware
      } else if (devName.containsIgnoreCase("USB") ||
                 devName.containsIgnoreCase("DAC") ||
                 devName.containsIgnoreCase("500R8")) {
        score = 3;
      } else if (devName.containsIgnoreCase("Default") ||
                 devName.containsIgnoreCase("hdmi") ||
                 devName.containsIgnoreCase("bcm2835")) {
        score = 0;
      }

      if (score > highestScore) {
        highestScore = score;
        bestOutputDevice = devName;
      }
    }
  }

  juce::AudioDeviceManager::AudioDeviceSetup setup;
  deviceManager.getAudioDeviceSetup(setup);
  setup.outputChannels = 2;
  setup.inputChannels = 0;
  setup.sampleRate = 48000.0;
  setup.bufferSize = 128; // Low latency buffer

  if (bestOutputDevice.isNotEmpty()) {
    setup.outputDeviceName = bestOutputDevice;
  }

  juce::String error = deviceManager.setAudioDeviceSetup(setup, true);
  if (error.isNotEmpty()) {
    juce::Logger::writeToLog(
        "AudioEngine Error: Failed to open hardware audio device - " + error);
    deviceManager.initialiseWithDefaultDevices(0, 2);
  }

  auto *currentDevice = deviceManager.getCurrentAudioDevice();
  if (currentDevice != nullptr && currentDevice->isPlaying()) {
    audioDeviceOK = true;
    juce::Logger::writeToLog(
        "AudioEngine Success: Active Audio Output -> " +
        currentDevice->getName() + " [" +
        juce::String(currentDevice->getCurrentSampleRate()) + " Hz, " +
        juce::String(currentDevice->getCurrentBufferSizeSamples()) +
        " samples]");
  }

  deviceManager.removeAudioCallback(this);
  deviceManager.addAudioCallback(this);

  // Register ChangeListener on deviceManager for live MIDI device toggling
  deviceManager.addChangeListener(this);
  refreshMidiInputs();
  sendChangeMessage();

  return audioDeviceOK;
}

// =============================================================================
// Live MIDI Hotplug Device Scanner & Callback Registration
// =============================================================================
void AudioEngine::refreshMidiInputs() {
  auto midiInputs = juce::MidiInput::getAvailableDevices();
  for (const auto &input : midiInputs) {
    if (!deviceManager.isMidiInputDeviceEnabled(input.identifier)) {
      deviceManager.setMidiInputDeviceEnabled(input.identifier, true);
    }
    // Always bind AudioEngine callback recipient to all enabled MIDI inputs
    if (deviceManager.isMidiInputDeviceEnabled(input.identifier)) {
      deviceManager.addMidiInputDeviceCallback(input.identifier, this);
      juce::Logger::writeToLog("Connected MIDI Input Device: " + input.name);
    }
  }
}

void AudioEngine::changeListenerCallback(juce::ChangeBroadcaster* /*source*/) {
  refreshMidiInputs();
}

juce::String AudioEngine::getActiveAudioDeviceName() const {
  auto *device = deviceManager.getCurrentAudioDevice();
  return device != nullptr ? device->getName() : "None";
}

void AudioEngine::shutdown() {
  deviceManager.removeChangeListener(this);

  auto midiInputs = juce::MidiInput::getAvailableDevices();
  for (const auto &input : midiInputs) {
    deviceManager.removeMidiInputDeviceCallback(input.identifier, this);
  }

  deviceManager.removeAudioCallback(this);
}

bool AudioEngine::setBufferSize(int newBufferSize) {
  if (newBufferSize <= 0)
    return false;
  juce::AudioDeviceManager::AudioDeviceSetup setup;
  deviceManager.getAudioDeviceSetup(setup);
  setup.bufferSize = newBufferSize;
  juce::String error = deviceManager.setAudioDeviceSetup(setup, true);
  if (error.isEmpty()) {
    juce::Logger::writeToLog("AudioEngine Success: Buffer size updated -> " +
                             juce::String(newBufferSize) + " samples");
    sendChangeMessage();
    return true;
  }
  juce::Logger::writeToLog("AudioEngine Error: Failed to set buffer size " +
                           juce::String(newBufferSize) + " - " + error);
  return false;
}

// =============================================================================
// MIDI Message Handling (Hardware + External Network UDP)
// =============================================================================
void AudioEngine::postExternalMidiMessage(const juce::MidiMessage &message) {
  handleIncomingMidiMessage(nullptr, message);
}

void AudioEngine::handleIncomingMidiMessage(juce::MidiInput * /*source*/,
                                            const juce::MidiMessage &message) {
  if (message.isNoteOn()) {
    midiState.noteOn(message.getNoteNumber(), message.getFloatVelocity());
  } else if (message.isNoteOff()) {
    midiState.noteOff(message.getNoteNumber());
  } else if (message.isController() && message.getControllerNumber() == 64) {
    bool pedalDown = (message.getControllerValue() >= 64);
    midiState.setSustainPedal(pedalDown);
  }

  // Thread-safe broadcast of MIDI callback to all registered listeners & terminal stdout
  juce::MessageManager::callAsync([this, message] {
    std::cout << "[MIDI HARDWARE INPUT] " << message.getDescription().toStdString() << std::endl;
    for (const auto &listener : midiListeners) {
      if (listener != nullptr) {
        listener(message);
      }
    }
  });

  const juce::ScopedLock sl(midiLock);
  incomingMidiBuffer.addEvent(message, 0);
}

// =============================================================================
// Audio Device Callbacks & Pipeline Processing
// =============================================================================
void AudioEngine::audioDeviceAboutToStart(juce::AudioIODevice *device) {
  if (device != nullptr) {
    double sr = device->getCurrentSampleRate();
    int bs = device->getCurrentBufferSizeSamples();

    synth.prepareToPlay(sr, bs);
    fxChannel.prepareToPlay(sr, bs);
    masterChain.prepareToPlay(sr, bs);

    // Pre-allocate AUX send buffer capacity ONCE (Zero Heap Allocation)
    auxBuffer.setSize(2, juce::jmax(512, bs), false, false, true);
  }
}

void AudioEngine::audioDeviceStopped() {}

void AudioEngine::audioDeviceIOCallbackWithContext(
    const float *const * /*inputChannelData*/, int /*numInputChannels*/,
    float *const *outputChannelData, int numOutputChannels, int numSamples,
    const juce::AudioIODeviceCallbackContext & /*context*/) {

  juce::AudioBuffer<float> buffer(outputChannelData, numOutputChannels,
                                  numSamples);
  buffer.clear();

  // Clear AUX send buffer
  auxBuffer.clear(0, numSamples);

  // Thread-safe copy of incoming MIDI buffer
  juce::MidiBuffer midiMessagesToProcess;
  {
    const juce::ScopedLock sl(midiLock);
    midiMessagesToProcess.addEvents(incomingMidiBuffer, 0, numSamples, 0);
    incomingMidiBuffer.clear();
  }

  // ── Step 1: Render LayeredSynth (4 Layers -> buffer, AUX sends -> auxBuffer)
  synth.renderNextBlock(buffer, midiMessagesToProcess, 0, numSamples,
                        &auxBuffer);

  // ── Step 2: Process FXChannel (Reverb + Delay -> sums to buffer)
  fxChannel.processBlock(auxBuffer, buffer, 0, numSamples);

  // ── Step 3: Process MasterChain (Compressor -> Limiter -> Clipper -> Gain -> VU Meter)
  masterChain.processBlock(buffer, 0, numSamples);
}
