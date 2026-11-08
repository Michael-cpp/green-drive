#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using Float = juce::AudioParameterFloat;
    using Id = juce::ParameterID;
    // Knobs store 0..1 but read 0..10.
    const auto dial = juce::AudioParameterFloatAttributes()
                          .withStringFromValueFunction ([] (float v, int) { return juce::String (v * 10.0f, 1); })
                          .withValueFromStringFunction ([] (const juce::String& s) { return juce::jlimit (0.0f, 1.0f, s.getFloatValue() / 10.0f); });

    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::make_unique<Float> (Id { "drive", 1 }, "Drive", juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f, dial));
    layout.add (std::make_unique<Float> (Id { "tone", 1 }, "Tone", juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f, dial));
    layout.add (std::make_unique<Float> (Id { "level", 1 }, "Level", juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f, dial));
    // Sets how hot the DAW signal looks to the circuit. Input is scaled by
    // this amount and output by its inverse. 0 dB means 0 dBFS = 1 V peak.
    layout.add (std::make_unique<Float> (Id { "calibration", 1 }, "Input Calibration",
                                         juce::NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f));
    layout.add (std::make_unique<juce::AudioParameterBool> (Id { "bypass", 1 }, "Bypass", false));
    return layout;
}
} // namespace

GreenDriveProcessor::GreenDriveProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      params (*this, nullptr, "PARAMS", createLayout())
{
}

bool GreenDriveProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void GreenDriveProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const auto channels = (size_t) juce::jmax (1, getTotalNumInputChannels());
    oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
        channels, 2, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
    oversampling->initProcessing ((size_t) samplesPerBlock);
    oversampling->reset();
    setLatencySamples ((int) std::lround (oversampling->getLatencyInSamples()));

    for (auto& p : pedals)
        p.prepare (sampleRate * 4.0);

    const auto reset = [&] (juce::SmoothedValue<float>& v, const char* id)
    {
        v.reset (sampleRate, 0.02);
        v.setCurrentAndTargetValue (*params.getRawParameterValue (id));
    };
    reset (drive, "drive");
    reset (tone, "tone");
    reset (level, "level");
    reset (calibration, "calibration");
    wetMix.reset (sampleRate * 4.0, 0.01);
    wetMix.setCurrentAndTargetValue (*params.getRawParameterValue ("bypass") > 0.5f ? 0.0f : 1.0f);
    updateControls();
}

void GreenDriveProcessor::updateControls()
{
    const auto d = (double) drive.getCurrentValue();
    const auto t = (double) tone.getCurrentValue();
    const auto l = (double) level.getCurrentValue();
    for (auto& p : pedals)
        p.setControls (d, t, l);
}

void GreenDriveProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (auto ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    drive.setTargetValue (*params.getRawParameterValue ("drive"));
    tone.setTargetValue (*params.getRawParameterValue ("tone"));
    level.setTargetValue (*params.getRawParameterValue ("level"));
    calibration.setTargetValue (*params.getRawParameterValue ("calibration"));
    wetMix.setTargetValue (*params.getRawParameterValue ("bypass") > 0.5f ? 0.0f : 1.0f);

    juce::dsp::AudioBlock<float> block (buffer);
    block = block.getSubsetChannelBlock (0, (size_t) getTotalNumInputChannels());
    auto up = oversampling->processSamplesUp (block);

    const auto numSamples = (int) up.getNumSamples();
    const auto numChannels = (int) up.getNumChannels();
    const int osInterval = controlInterval * 4;

    double inGain = 1.0, outGain = 1.0;

    for (int n = 0; n < numSamples; ++n)
    {
        if (n % osInterval == 0)
        {
            drive.skip (controlInterval);
            tone.skip (controlInterval);
            level.skip (controlInterval);
            calibration.skip (controlInterval);
            updateControls();
            inGain = juce::Decibels::decibelsToGain ((double) calibration.getCurrentValue());
            outGain = 1.0 / inGain;
        }

        // Dry passes through the same oversampling filters, so it stays time-aligned.
        const float mix = wetMix.getNextValue();

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* samples = up.getChannelPointer ((size_t) ch);
            const float dry = samples[n];
            const auto wet = (float) (pedals[(size_t) ch].process (dry * inGain) * outGain);
            samples[n] = dry + mix * (wet - dry);
        }
    }

    oversampling->processSamplesDown (block);
}

juce::AudioProcessorEditor* GreenDriveProcessor::createEditor()
{
    return new GreenDriveEditor (*this);
}

void GreenDriveProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = params.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void GreenDriveProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (params.state.getType()))
            params.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GreenDriveProcessor();
}
