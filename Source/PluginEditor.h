#pragma once

#include <memory>

#include <juce_audio_utils/juce_audio_utils.h>

#include "PluginProcessor.h"

class PedalLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float position, float startAngle, float endAngle, juce::Slider&) override;
};

class Footswitch : public juce::Button
{
public:
    Footswitch() : juce::Button ("Footswitch") { setClickingTogglesState (true); }

    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
};

class GreenDriveEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit GreenDriveEditor (GreenDriveProcessor&);
    ~GreenDriveEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void timerCallback() override;
    void setUpKnob (juce::Slider&, const char* paramId, std::unique_ptr<SliderAttachment>&);

    GreenDriveProcessor& processor;
    PedalLookAndFeel lookAndFeel;

    juce::Slider drive, tone, level, calibration;
    Footswitch footswitch;

    std::unique_ptr<SliderAttachment> driveAttachment, toneAttachment, levelAttachment, calibrationAttachment;
    std::unique_ptr<ButtonAttachment> bypassAttachment;

    juce::Rectangle<float> ledArea;
    bool ledOn = true;
    juce::Image face;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GreenDriveEditor)
};
