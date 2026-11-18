#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>

class VoxRestoreAudioProcessor : public juce::AudioProcessor
{
public:
    VoxRestoreAudioProcessor();
    ~VoxRestoreAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "VOX RE:STORE"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState parameters;
    void startAutoAnalysis();
    bool isAutoAnalysisComplete() const { return autoComplete.load(); }
    void applyAutoAnalysis();
    float getAutoProgress() const;

private:
    using APVTS = juce::AudioProcessorValueTreeState;
    using Filter = juce::dsp::IIR::Filter<float>;
    static APVTS::ParameterLayout createParameterLayout();
    float value(const char* id) const;

    double sr = 44100.0;
    std::array<Filter, 2> hp, metalBand, essBand, fizzBand, bodyShelf;
    std::array<juce::SmoothedValue<float>, 16> smoothParams;
    std::array<float, 2> metalEnv {}, essEnv {}, fizzEnv {}, rmsEnv {};
    std::atomic<bool> autoRunning { false }, autoComplete { false };
    std::atomic<int64_t> autoSamples { 0 };
    std::atomic<float> autoProgress { 0.0f };
    double autoSumSq = 0.0, autoHighSq = 0.0, autoEssSq = 0.0;
    float autoPeak = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VoxRestoreAudioProcessor)
};
