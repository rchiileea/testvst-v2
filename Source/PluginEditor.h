#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class VoxRestoreAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit VoxRestoreAudioProcessorEditor(VoxRestoreAudioProcessor&);
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    VoxRestoreAudioProcessor& processor;
    juce::ComboBox presetBox;
    juce::TextButton autoButton { "AUTO • LISTEN 10s" }, powerButton { "POWER / A-B" };
    juce::Label status;
    juce::OwnedArray<juce::Slider> sliders;
    juce::OwnedArray<juce::Label> labels;
    std::vector<std::unique_ptr<Attachment>> attachments;
    const juce::String ids[16] = { "input","lowcut","noise","gaps","metal","harsh","deess",
        "metalfocus","sensitivity","sibfocus","warmth","body","defizz","level","output","mix" };
    const juce::String names[16] = { "INPUT TRIM","LOW CUT","NOISE FLOOR","GAP REDUCTION",
        "METALLIC REMOVAL","HARSHNESS","SIBILANCE","METAL FOCUS","SENSITIVITY","SIBILANT FOCUS",
        "WARMTH","BODY","DE-FIZZ","LEVELING","OUTPUT","MIX" };
    void timerCallback() override;
    void setPreset(int);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VoxRestoreAudioProcessorEditor)
};
