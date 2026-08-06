#include "FXCardComponent.h"

FXCardComponent::FXCardComponent(AudioEngine& engineToControl)
    : audioEngine(engineToControl) {

    addAndMakeVisible(headerLabel);
    headerLabel.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    headerLabel.setColour(juce::Label::textColourId, juce::Colour(0xff38bdf8)); // Cyan

    // ── 1. Delay Controls (Delay First) ───────────────────────────────────────
    addAndMakeVisible(delayHeaderLabel);
    delayHeaderLabel.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    delayHeaderLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);

    addAndMakeVisible(delayToggle);
    delayToggle.setToggleState(true, juce::dontSendNotification);
    delayToggle.onClick = [this] {
        audioEngine.getFXChannel().setDelayEnabled(delayToggle.getToggleState());
    };

    auto setupRotary = [this](juce::Slider& s, juce::Label& l, double minV, double maxV, double defV) {
        addAndMakeVisible(s);
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 48, 16);
        s.setRange(minV, maxV, 0.01);
        s.setValue(defV);

        addAndMakeVisible(l);
        l.setFont(juce::FontOptions(9.0f));
        l.setJustificationType(juce::Justification::centred);
    };

    setupRotary(delayTimeSlider, delayTimeLabel, 1.0, 2000.0, 375.0);
    delayTimeSlider.onValueChange = [this] {
        audioEngine.getFXChannel().setDelayTimeMs((float)delayTimeSlider.getValue());
    };

    setupRotary(delayFeedbackSlider, delayFeedbackLabel, 0.0, 0.95, 0.35);
    delayFeedbackSlider.onValueChange = [this] {
        audioEngine.getFXChannel().setDelayFeedback((float)delayFeedbackSlider.getValue());
    };

    setupRotary(delayWetSlider, delayWetLabel, 0.0, 1.0, 0.30);
    delayWetSlider.onValueChange = [this] {
        audioEngine.getFXChannel().setDelayWetLevel((float)delayWetSlider.getValue());
    };

    // ── 2. Reverb Controls (Reverb Second) ────────────────────────────────────
    addAndMakeVisible(reverbHeaderLabel);
    reverbHeaderLabel.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    reverbHeaderLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);

    addAndMakeVisible(reverbToggle);
    reverbToggle.setToggleState(true, juce::dontSendNotification);
    reverbToggle.onClick = [this] {
        audioEngine.getFXChannel().setReverbEnabled(reverbToggle.getToggleState());
    };

    setupRotary(reverbRoomSlider, reverbRoomLabel, 0.0, 1.0, 0.50);
    reverbRoomSlider.onValueChange = [this] {
        audioEngine.getFXChannel().setReverbRoomSize((float)reverbRoomSlider.getValue());
    };

    setupRotary(reverbDampingSlider, reverbDampingLabel, 0.0, 1.0, 0.50);
    reverbDampingSlider.onValueChange = [this] {
        audioEngine.getFXChannel().setReverbDamping((float)reverbDampingSlider.getValue());
    };

    setupRotary(reverbWetSlider, reverbWetLabel, 0.0, 1.0, 0.33);
    reverbWetSlider.onValueChange = [this] {
        audioEngine.getFXChannel().setReverbWetLevel((float)reverbWetSlider.getValue());
    };

    // Master FX Output Slider
    addAndMakeVisible(fxOutputSlider);
    fxOutputSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    fxOutputSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 45, 18);
    fxOutputSlider.setRange(0.0, 2.0, 0.01);
    fxOutputSlider.setValue(1.0);
    fxOutputSlider.onValueChange = [this] {
        audioEngine.getFXChannel().setFxOutputGain((float)fxOutputSlider.getValue());
    };

    addAndMakeVisible(fxOutputLabel);
    fxOutputLabel.setFont(juce::FontOptions(10.0f));
}

void FXCardComponent::updateFromPreset() {
    delayToggle.setToggleState(audioEngine.getFXChannel().isDelayEnabled(), juce::dontSendNotification);
    delayTimeSlider.setValue(audioEngine.getFXChannel().getDelayTimeMs(), juce::dontSendNotification);
    delayFeedbackSlider.setValue(audioEngine.getFXChannel().getDelayFeedback(), juce::dontSendNotification);
    delayWetSlider.setValue(audioEngine.getFXChannel().getDelayWetLevel(), juce::dontSendNotification);

    reverbToggle.setToggleState(audioEngine.getFXChannel().isReverbEnabled(), juce::dontSendNotification);
    reverbRoomSlider.setValue(audioEngine.getFXChannel().getReverbRoomSize(), juce::dontSendNotification);
    reverbDampingSlider.setValue(audioEngine.getFXChannel().getReverbDamping(), juce::dontSendNotification);
    reverbWetSlider.setValue(audioEngine.getFXChannel().getReverbWetLevel(), juce::dontSendNotification);
    fxOutputSlider.setValue(audioEngine.getFXChannel().getFxOutputGain(), juce::dontSendNotification);
}

void FXCardComponent::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat().reduced(2.0f);
    g.setColour(juce::Colour(0xff18181b)); // Dark card background
    g.fillRoundedRectangle(bounds, 8.0f);

    g.setColour(juce::Colour(0xff27272a)); // Card border
    g.drawRoundedRectangle(bounds, 8.0f, 1.5f);
}

void FXCardComponent::resized() {
    auto bounds = getLocalBounds().reduced(10);

    headerLabel.setBounds(bounds.removeFromTop(24));
    bounds.removeFromTop(6);

    // Section 1: Delay (Top)
    auto delaySec = bounds.removeFromTop(105);
    auto dHead = delaySec.removeFromTop(20);
    delayHeaderLabel.setBounds(dHead.removeFromLeft(120));
    delayToggle.setBounds(dHead);

    int rotWidth = delaySec.getWidth() / 3;
    auto d1 = delaySec.removeFromLeft(rotWidth);
    delayTimeLabel.setBounds(d1.removeFromTop(14));
    delayTimeSlider.setBounds(d1);

    auto d2 = delaySec.removeFromLeft(rotWidth);
    delayFeedbackLabel.setBounds(d2.removeFromTop(14));
    delayFeedbackSlider.setBounds(d2);

    auto d3 = delaySec;
    delayWetLabel.setBounds(d3.removeFromTop(14));
    delayWetSlider.setBounds(d3);

    bounds.removeFromTop(8);

    // Section 2: Reverb (Bottom)
    auto reverbSec = bounds.removeFromTop(105);
    auto rHead = reverbSec.removeFromTop(20);
    reverbHeaderLabel.setBounds(rHead.removeFromLeft(120));
    reverbToggle.setBounds(rHead);

    auto r1 = reverbSec.removeFromLeft(rotWidth);
    reverbRoomLabel.setBounds(r1.removeFromTop(14));
    reverbRoomSlider.setBounds(r1);

    auto r2 = reverbSec.removeFromLeft(rotWidth);
    reverbDampingLabel.setBounds(r2.removeFromTop(14));
    reverbDampingSlider.setBounds(r2);

    auto r3 = reverbSec;
    reverbWetLabel.setBounds(r3.removeFromTop(14));
    reverbWetSlider.setBounds(r3);

    bounds.removeFromTop(8);

    // Master FX Output Slider
    fxOutputLabel.setBounds(bounds.removeFromTop(16));
    fxOutputSlider.setBounds(bounds);
}
