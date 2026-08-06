#include "MasterChannelStrip.h"

MasterChannelStrip::MasterChannelStrip(AudioEngine& engineToControl, PresetManager& presetTarget)
    : audioEngine(engineToControl), presetManager(presetTarget) {

    addAndMakeVisible(headerLabel);
    headerLabel.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    headerLabel.setColour(juce::Label::textColourId, juce::Colour(0xfff59e0b)); // Amber

    const auto& mc = presetManager.getMasterChainPreset();
    auto& masterChain = audioEngine.getMasterChain();

    // ── 1. Compressor ────────────────────────────────────────────────────────
    addAndMakeVisible(compHeaderLabel);
    compHeaderLabel.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    compHeaderLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);

    addAndMakeVisible(compToggle);
    compToggle.setToggleState(mc.compEnabled, juce::dontSendNotification);
    compToggle.onClick = [this] {
        audioEngine.getMasterChain().setCompressorEnabled(compToggle.getToggleState());
    };

    auto setupRotary = [this](juce::Slider& s, juce::Label& l, double minV, double maxV, double defV) {
        addAndMakeVisible(s);
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 45, 14);
        s.setRange(minV, maxV, 0.1);
        s.setValue(defV);

        addAndMakeVisible(l);
        l.setFont(juce::FontOptions(9.0f));
        l.setJustificationType(juce::Justification::centred);
    };

    setupRotary(compThresholdSlider, compThresholdLabel, -60.0, 0.0, mc.compThresholdDb);
    compThresholdSlider.onValueChange = [this] {
        audioEngine.getMasterChain().setCompressorThreshold((float)compThresholdSlider.getValue());
    };

    setupRotary(compRatioSlider, compRatioLabel, 1.0, 20.0, mc.compRatio);
    compRatioSlider.onValueChange = [this] {
        audioEngine.getMasterChain().setCompressorRatio((float)compRatioSlider.getValue());
    };

    // ── 2. Limiter ───────────────────────────────────────────────────────────
    addAndMakeVisible(limHeaderLabel);
    limHeaderLabel.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    limHeaderLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);

    addAndMakeVisible(limToggle);
    limToggle.setToggleState(mc.limEnabled, juce::dontSendNotification);
    limToggle.onClick = [this] {
        audioEngine.getMasterChain().setLimiterEnabled(limToggle.getToggleState());
    };

    setupRotary(limThresholdSlider, limThresholdLabel, -24.0, 0.0, mc.limThresholdDb);
    limThresholdSlider.onValueChange = [this] {
        audioEngine.getMasterChain().setLimiterThreshold((float)limThresholdSlider.getValue());
    };

    // ── 3. Soft Clipper ───────────────────────────────────────────────────────
    addAndMakeVisible(clipHeaderLabel);
    clipHeaderLabel.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    clipHeaderLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);

    addAndMakeVisible(clipToggle);
    clipToggle.setToggleState(mc.clipEnabled, juce::dontSendNotification);
    clipToggle.onClick = [this] {
        audioEngine.getMasterChain().setClipperEnabled(clipToggle.getToggleState());
    };

    setupRotary(clipDriveSlider, clipDriveLabel, 0.0, 24.0, mc.clipDriveDb);
    clipDriveSlider.onValueChange = [this] {
        audioEngine.getMasterChain().setClipperDrive((float)clipDriveSlider.getValue());
    };

    // ── Master Output Fader ───────────────────────────────────────────────────
    addAndMakeVisible(masterFader);
    masterFader.setSliderStyle(juce::Slider::LinearVertical);
    masterFader.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 18);
    masterFader.setRange(0.0, 2.0, 0.01);
    masterFader.setValue(mc.masterGain);
    masterFader.onValueChange = [this] {
        audioEngine.getMasterChain().setMasterGain((float)masterFader.getValue());
    };

    addAndMakeVisible(masterFaderLabel);
    masterFaderLabel.setFont(juce::FontOptions(10.0f));
    masterFaderLabel.setJustificationType(juce::Justification::centred);

    startTimerHz(30); // 30 Hz VU Meter Refresh
}

MasterChannelStrip::~MasterChannelStrip() {
    stopTimer();
}

void MasterChannelStrip::timerCallback() {
    currentPeakL = audioEngine.getMasterChain().getPeakLevelL();
    currentPeakR = audioEngine.getMasterChain().getPeakLevelR();
    repaint();
}

void MasterChannelStrip::updateFromPreset() {
    const auto& mc = presetManager.getMasterChainPreset();
    compToggle.setToggleState(mc.compEnabled, juce::dontSendNotification);
    compThresholdSlider.setValue(mc.compThresholdDb, juce::dontSendNotification);
    compRatioSlider.setValue(mc.compRatio, juce::dontSendNotification);

    limToggle.setToggleState(mc.limEnabled, juce::dontSendNotification);
    limThresholdSlider.setValue(mc.limThresholdDb, juce::dontSendNotification);

    clipToggle.setToggleState(mc.clipEnabled, juce::dontSendNotification);
    clipDriveSlider.setValue(mc.clipDriveDb, juce::dontSendNotification);

    masterFader.setValue(mc.masterGain, juce::dontSendNotification);
}

void MasterChannelStrip::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat().reduced(2.0f);
    g.setColour(juce::Colour(0xff18181b));
    g.fillRoundedRectangle(bounds, 8.0f);

    g.setColour(juce::Colour(0xff27272a));
    g.drawRoundedRectangle(bounds, 8.0f, 1.5f);

    // Draw VU Meter Bars on the right side of master fader
    auto meterArea = getLocalBounds().removeFromRight(20).reduced(4, 20);
    g.setColour(juce::Colour(0xff09090b));
    g.fillRect(meterArea);

    float hL = meterArea.getHeight() * juce::jlimit(0.0f, 1.0f, currentPeakL);
    float hR = meterArea.getHeight() * juce::jlimit(0.0f, 1.0f, currentPeakR);

    // Left Peak Bar (Green to Red gradient)
    juce::Rectangle<float> barL(meterArea.getX(), meterArea.getBottom() - hL, 5, hL);
    g.setColour((currentPeakL > 0.95f) ? juce::Colours::red : juce::Colour(0xff10b981));
    g.fillRect(barL);

    // Right Peak Bar
    juce::Rectangle<float> barR(meterArea.getX() + 7, meterArea.getBottom() - hR, 5, hR);
    g.setColour((currentPeakR > 0.95f) ? juce::Colours::red : juce::Colour(0xff10b981));
    g.fillRect(barR);
}

void MasterChannelStrip::resized() {
    auto bounds = getLocalBounds().reduced(8);

    headerLabel.setBounds(bounds.removeFromTop(24));
    bounds.removeFromTop(4);

    // 1. Compressor
    auto compBox = bounds.removeFromTop(75);
    auto cHead = compBox.removeFromTop(18);
    compHeaderLabel.setBounds(cHead.removeFromLeft(80));
    compToggle.setBounds(cHead);

    int rotW = compBox.getWidth() / 2;
    auto c1 = compBox.removeFromLeft(rotW);
    compThresholdLabel.setBounds(c1.removeFromTop(12));
    compThresholdSlider.setBounds(c1);

    auto c2 = compBox;
    compRatioLabel.setBounds(c2.removeFromTop(12));
    compRatioSlider.setBounds(c2);

    bounds.removeFromTop(4);

    // 2. Limiter
    auto limBox = bounds.removeFromTop(65);
    auto lHead = limBox.removeFromTop(18);
    limHeaderLabel.setBounds(lHead.removeFromLeft(80));
    limToggle.setBounds(lHead);

    limThresholdLabel.setBounds(limBox.removeFromTop(12));
    limThresholdSlider.setBounds(limBox);

    bounds.removeFromTop(4);

    // 3. Soft Clipper
    auto clipBox = bounds.removeFromTop(65);
    auto clHead = clipBox.removeFromTop(18);
    clipHeaderLabel.setBounds(clHead.removeFromLeft(90));
    clipToggle.setBounds(clHead);

    clipDriveLabel.setBounds(clipBox.removeFromTop(12));
    clipDriveSlider.setBounds(clipBox);

    bounds.removeFromTop(6);

    // Master Fader
    auto faderArea = bounds.removeFromLeft(bounds.getWidth() - 24);
    masterFaderLabel.setBounds(faderArea.removeFromTop(16));
    masterFader.setBounds(faderArea);
}
