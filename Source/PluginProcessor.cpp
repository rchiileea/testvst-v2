#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr float pi = 3.14159265358979323846f;
    constexpr int paramCount = 16;
    const char* paramIDs[paramCount] = { "input","lowcut","noise","gaps","metal","harsh","deess",
        "metalfocus","sensitivity","sibfocus","warmth","body","defizz","level","output","mix" };

    float dbToGain(float db) { return juce::Decibels::decibelsToGain(db); }
    float clamp01(float x) { return juce::jlimit(0.0f, 1.0f, x); }
    float envelope(float input, float previous, float attack, float release)
    {
        const float c = input > previous ? attack : release;
        return input + c * (previous - input);
    }
}

VoxRestoreAudioProcessor::VoxRestoreAudioProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

VoxRestoreAudioProcessor::APVTS::ParameterLayout VoxRestoreAudioProcessor::createParameterLayout()
{
    APVTS::ParameterLayout p;
    auto add = [&p](const char* id, const char* name, float lo, float hi, float def, float step = 0.0f)
    {
        p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id, 1}, name,
            juce::NormalisableRange<float>(lo, hi, step), def));
    };
    add("input", "Input Trim", -18, 18, 0, 0.1f);
    add("lowcut", "Low Cut", 20, 180, 45, 1);
    add("noise", "Noise Floor", 0, 100, 12, 1);
    add("gaps", "Gap Reduction", 0, 100, 8, 1);
    add("metal", "Metallic Removal", 0, 100, 24, 1);
    add("harsh", "Harshness Removal", 0, 100, 20, 1);
    add("deess", "Sibilance", 0, 100, 24, 1);
    add("metalfocus", "Metal Focus", 0, 100, 55, 1);
    add("sensitivity", "Sensitivity", 0, 100, 48, 1);
    add("sibfocus", "Sibilant Focus", 0, 100, 62, 1);
    add("warmth", "Warmth", 0, 100, 12, 1);
    add("body", "Body", 0, 100, 12, 1);
    add("defizz", "De-fizz", 0, 100, 18, 1);
    add("level", "Leveling", 0, 100, 18, 1);
    add("output", "Output", -18, 18, 0, 0.1f);
    add("mix", "Mix", 0, 100, 100, 1);
    add("power", "Power", 0, 1, 1, 1);
    return p;
}

float VoxRestoreAudioProcessor::value(const char* id) const
{
    if (auto* v = parameters.getRawParameterValue(id)) return v->load();
    return 0.0f;
}

void VoxRestoreAudioProcessor::prepareToPlay(double sampleRate, int)
{
    sr = sampleRate;
    juce::dsp::ProcessSpec spec { sr, 1, 1 };
    for (int ch = 0; ch < 2; ++ch)
    {
        hp[ch].reset(); metalBand[ch].reset(); essBand[ch].reset();
        fizzBand[ch].reset(); bodyShelf[ch].reset();
    }
    auto setFilter = [&](auto& filters, auto coeff)
    {
        for (auto& f : filters) { f.coefficients = coeff; f.prepare(spec); }
    };
    setFilter(hp, juce::dsp::IIR::Coefficients<float>::makeHighPass(sr, 45.0, 0.707));
    setFilter(metalBand, juce::dsp::IIR::Coefficients<float>::makeBandPass(sr, 3600.0, 1.2));
    setFilter(essBand, juce::dsp::IIR::Coefficients<float>::makeBandPass(sr, 7200.0, 0.85));
    setFilter(fizzBand, juce::dsp::IIR::Coefficients<float>::makeHighPass(sr, 9500.0, 0.707));
    setFilter(bodyShelf, juce::dsp::IIR::Coefficients<float>::makeLowShelf(sr, 190.0, 0.7, 1.0));

    const float initial[paramCount] = {0,45,12,8,24,20,24,55,48,62,12,12,18,18,0,100};
    for (int i = 0; i < paramCount; ++i)
    {
        smoothParams[(size_t)i].reset(sr, 0.025);
        smoothParams[(size_t)i].setCurrentAndTargetValue(initial[i]);
    }
    metalEnv.fill(0); essEnv.fill(0); fizzEnv.fill(0); rmsEnv.fill(0);
}

void VoxRestoreAudioProcessor::releaseResources() {}

bool VoxRestoreAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    return in == layouts.getMainOutputChannelSet() &&
           (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo());
}

void VoxRestoreAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int channels = juce::jmin(2, getTotalNumInputChannels());
    const int n = buffer.getNumSamples();

    for (int i = 0; i < paramCount; ++i)
        smoothParams[(size_t)i].setTargetValue(value(paramIDs[i]));

    const float power = value("power");
    const float inputDb = smoothParams[0].getCurrentValue();
    juce::ignoreUnused(inputDb);

    const float hpHz = smoothParams[1].getTargetValue();
    // Update the HP coefficients at block rate; coefficient objects are reused by JUCE.
    auto hpCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass(sr, hpHz, 0.707);
    for (auto& f : hp) f.coefficients = hpCoeffs;

    const float attack = std::exp(-1.0f / (0.008f * (float)sr));
    const float release = std::exp(-1.0f / (0.085f * (float)sr));
    const float gateAttack = std::exp(-1.0f / (0.018f * (float)sr));
    const float gateRelease = std::exp(-1.0f / (0.120f * (float)sr));

    for (int ch = 0; ch < channels; ++ch)
    {
        auto* data = buffer.getWritePointer(ch);
        for (int i = 0; i < n; ++i)
        {
            float dry = data[i];
            float y = dry;
            if (power >= 0.5f)
            {
                const float inGain = dbToGain(smoothParams[0].getNextValue());
                const float noise = smoothParams[2].getNextValue() * 0.01f;
                const float gap = smoothParams[3].getNextValue() * 0.01f;
                const float metal = smoothParams[4].getNextValue() * 0.01f;
                const float harsh = smoothParams[5].getNextValue() * 0.01f;
                const float deess = smoothParams[6].getNextValue() * 0.01f;
                const float metalFocus = smoothParams[7].getNextValue() * 0.01f;
                const float sensitivity = smoothParams[8].getNextValue() * 0.01f;
                const float sibFocus = smoothParams[9].getNextValue() * 0.01f;
                const float warmth = smoothParams[10].getNextValue() * 0.01f;
                const float body = smoothParams[11].getNextValue() * 0.01f;
                const float fizz = smoothParams[12].getNextValue() * 0.01f;
                const float leveling = smoothParams[13].getNextValue() * 0.01f;
                const float outGain = dbToGain(smoothParams[14].getNextValue());
                const float mix = smoothParams[15].getNextValue() * 0.01f;

                y *= inGain;
                y = hp[ch].processSample(y);

                // Independent band-limited detectors isolate the material being reduced.
                const float metalSample = metalBand[ch].processSample(y);
                const float essSample = essBand[ch].processSample(y);
                const float fizzSample = fizzBand[ch].processSample(y);
                const float metalAbs = std::abs(metalSample);
                const float essAbs = std::abs(essSample);
                const float fizzAbs = std::abs(fizzSample);

                metalEnv[(size_t)ch] = envelope(metalAbs, metalEnv[(size_t)ch], attack, release);
                essEnv[(size_t)ch] = envelope(essAbs, essEnv[(size_t)ch], attack, release);
                fizzEnv[(size_t)ch] = envelope(fizzAbs, fizzEnv[(size_t)ch], attack, release);
                rmsEnv[(size_t)ch] = envelope(std::abs(y), rmsEnv[(size_t)ch], gateAttack, gateRelease);

                // Dynamic attenuation is limited to the detected frequency band.
                const float metalThreshold = 0.018f + (1.0f - sensitivity) * 0.045f;
                const float essThreshold = 0.012f + (1.0f - sibFocus) * 0.035f;
                const float metalOver = clamp01((metalEnv[(size_t)ch] - metalThreshold) * 14.0f);
                const float essOver = clamp01((essEnv[(size_t)ch] - essThreshold) * 18.0f);
                const float fizzOver = clamp01((fizzEnv[(size_t)ch] - 0.006f) * 30.0f);

                const float metalDepth = juce::jlimit(0.0f, 0.72f, metal * (0.18f + metalFocus * 0.58f) * metalOver);
                const float harshDepth = juce::jlimit(0.0f, 0.62f, harsh * 0.52f * metalOver);
                const float essDepth = juce::jlimit(0.0f, 0.82f, deess * (0.22f + sibFocus * 0.60f) * essOver);
                const float fizzDepth = juce::jlimit(0.0f, 0.78f, fizz * (0.20f + noise * 0.18f) * fizzOver);

                y -= metalSample * metalDepth;
                y -= metalSample * harshDepth;
                y -= essSample * essDepth;
                y -= fizzSample * fizzDepth;

                // Gentle downward expansion with a soft knee; does not hard-mute tails.
                const float floor = 0.0018f + noise * 0.0060f;
                const float below = clamp01((floor - rmsEnv[(size_t)ch]) / juce::jmax(0.0001f, floor));
                const float expanderGain = 1.0f - gap * 0.62f * below;
                y *= juce::jlimit(0.34f, 1.0f, expanderGain);

                // Low-shelf body and restrained saturation preserve pitch and formants.
                const float bodyBoost = 1.0f + body * 0.16f;
                const float lowBody = bodyShelf[ch].processSample(y);
                y = y + (lowBody - y) * body * 0.30f;
                const float drive = 1.0f + warmth * 1.8f;
                const float saturated = std::tanh(y * drive) / std::tanh(drive);
                y = y * (1.0f - warmth * 0.12f) + saturated * warmth * 0.12f;
                y *= bodyBoost;

                // Conservative peak levelling; no hard limiter or automatic makeup gain.
                const float levelAmount = leveling * 0.65f;
                const float target = 0.18f;
                const float gain = 1.0f / (1.0f + levelAmount * juce::jmax(0.0f, rmsEnv[(size_t)ch] - target) * 3.0f);
                y *= gain * outGain;
                y = dry + (y - dry) * mix;
            }
            data[i] = power >= 0.5f ? y : dry;
        }
    }

    for (int ch = channels; ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear(ch, 0, n);

    // Ten-second analysis of incoming audio. Measurements are deliberately simple and transparent.
    if (autoRunning.load())
    {
        double sum = 0.0, high = 0.0, ess = 0.0;
        float peak = 0.0f;
        for (int ch = 0; ch < channels; ++ch)
        {
            const auto* d = buffer.getReadPointer(ch);
            for (int i = 0; i < n; ++i)
            {
                const float s = d[i];
                sum += (double)s * s;
                peak = juce::jmax(peak, std::abs(s));
                if (std::abs(s) > 0.08f) high += (double)s * s;
                if (std::abs(s) > 0.12f && std::abs(s) < 0.95f)
                    ess += (double)s * s;
            }
        }
        autoSumSq += sum;
        autoHighSq += high;
        autoEssSq += ess;
        autoPeak = juce::jmax(autoPeak, peak);
        const int64_t total = autoSamples.fetch_add(n) + n;
        const int64_t goal = (int64_t)(sr * 10.0);
        autoProgress.store(clamp01((float)total / (float)goal));
        if (total >= goal)
        {
            autoRunning.store(false);
            autoComplete.store(true);
        }
    }
}

void VoxRestoreAudioProcessor::startAutoAnalysis()
{
    autoSumSq = autoHighSq = autoEssSq = 0.0;
    autoPeak = 0.0f;
    autoSamples.store(0);
    autoProgress.store(0.0f);
    autoComplete.store(false);
    autoRunning.store(true);
}

float VoxRestoreAudioProcessor::getAutoProgress() const { return autoProgress.load(); }

void VoxRestoreAudioProcessor::applyAutoAnalysis()
{
    if (!autoComplete.load()) return;
    const double denom = juce::jmax(1.0, (double)autoSamples.load() * (double)getTotalNumInputChannels());
    const float rms = (float)std::sqrt(autoSumSq / denom);
    const float highRatio = (float)std::sqrt(autoHighSq / juce::jmax(1.0, autoSumSq));
    const float essRatio = (float)std::sqrt(autoEssSq / juce::jmax(1.0, autoSumSq));
    auto set = [this](const char* id, float v)
    {
        if (auto* p = parameters.getParameter(id))
            p->setValueNotifyingHost(p->getNormalisableRange().convertTo0to1(v));
    };

    // Analysis suggests conservative starting values; it does not classify or reconstruct artifacts.
    set("input", juce::jlimit(-4.0f, 4.0f, -juce::Decibels::gainToDecibels(juce::jmax(0.2f, autoPeak))));
    set("lowcut", 45.0f);
    set("noise", juce::jlimit(5.0f, 35.0f, 8.0f + (rms < 0.07f ? 12.0f : 0.0f)));
    set("gaps", 8.0f);
    set("metal", juce::jlimit(12.0f, 48.0f, 16.0f + highRatio * 42.0f));
    set("harsh", juce::jlimit(10.0f, 42.0f, 12.0f + highRatio * 35.0f));
    set("deess", juce::jlimit(12.0f, 50.0f, 14.0f + essRatio * 40.0f));
    set("metalfocus", 54.0f);
    set("sensitivity", 48.0f);
    set("sibfocus", 62.0f);
    set("warmth", rms < 0.09f ? 16.0f : 9.0f);
    set("body", 12.0f);
    set("defizz", juce::jlimit(8.0f, 38.0f, 10.0f + highRatio * 34.0f));
    set("level", 18.0f);
    set("output", 0.0f);
    set("mix", 100.0f);
}

void VoxRestoreAudioProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, dest);
}

void VoxRestoreAudioProcessor::setStateInformation(const void* data, int size)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, size));
    if (xml && xml->hasTagName(parameters.state.getType()))
        parameters.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VoxRestoreAudioProcessor();
}

bool VoxRestoreAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* VoxRestoreAudioProcessor::createEditor()
{
    return new VoxRestoreAudioProcessorEditor (*this);
}
