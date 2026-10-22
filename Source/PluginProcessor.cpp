#include "PluginProcessor.h"
#include "PluginEditor.h"

BlgAudioProcessor::BlgAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", blg::createParameterLayout()),
      presets (apvts)
{
    pSampleRate = apvts.getRawParameterValue (blg::ids::sampleRate);
    pBits       = apvts.getRawParameterValue (blg::ids::bits);
    pTurbo      = apvts.getRawParameterValue (blg::ids::turbo);
    pTilt       = apvts.getRawParameterValue (blg::ids::tilt);
    pRevDry     = apvts.getRawParameterValue (blg::ids::revDry);
    pRevSize    = apvts.getRawParameterValue (blg::ids::revSize);
    pStereo     = apvts.getRawParameterValue (blg::ids::stereo);
    pRevWidth   = apvts.getRawParameterValue (blg::ids::revWidth);
    pRevDepth   = apvts.getRawParameterValue (blg::ids::revDepth);
    pFbAmount   = apvts.getRawParameterValue (blg::ids::fbAmount);
    pFbTilt     = apvts.getRawParameterValue (blg::ids::fbTilt);
    pFbShift    = apvts.getRawParameterValue (blg::ids::fbShift);
}

bool BlgAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo())
           && out == layouts.getMainInputChannelSet();
}

void BlgAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    degrade.prepare  (sampleRate, samplesPerBlock);
    spectral.prepare (sampleRate, samplesPerBlock);
    reverb.prepare   (sampleRate, samplesPerBlock);

    // ~170 ms of history at typical sample rates -- comfortably more than either visualiser
    // ever reads in one go, so a read always lands well behind the write position.
    const int scopeCapacity = juce::jmax (8192, samplesPerBlock * 4);
    outputScope.prepare     (2, scopeCapacity);
    spectralInScope.prepare (2, scopeCapacity);
    spectralOutScope.prepare (2, scopeCapacity);

    stereoSmoothed.reset (sampleRate, 0.03);
    stereoSmoothed.setCurrentAndTargetValue (pStereo->load());

    // the spectral feedback stage works on 1024-sample frames, so the whole plugin runs
    // fftSize - 1 samples late; this is constant whatever the settings, and hosts compensate
    setLatencySamples (blg::dsp::SpectralFeedback::latencySamples);
}

void BlgAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, numSamples);

    if (buffer.getNumChannels() == 0 || numSamples == 0)
        return;

    // 1) Vintage: Turbo -> sample rate / bits -> Tilt
    blg::dsp::DegradeEngine::Settings ds;
    ds.sampleRateHz = pSampleRate->load();
    ds.turbo        = pTurbo->load();
    ds.tiltDb       = pTilt->load();
    ds.bits         = blg::bitDepthTable[juce::jlimit (0, 3, juce::roundToInt (pBits->load()))];
    degrade.process (buffer, ds);

    // 2) Spectral feedback (Feedback panel)
    spectralInScope.push (buffer);
    blg::dsp::SpectralFeedback::Settings fs;
    fs.amount    = pFbAmount->load();
    fs.tilt      = pFbTilt->load();
    fs.shiftBins = juce::roundToInt (pFbShift->load());
    spectral.process (buffer, fs);
    spectralOutScope.push (buffer);

    // 3) Reverb
    blg::dsp::ReverbEngine::Settings rs;
    rs.dry   = pRevDry->load();
    rs.size  = pRevSize->load();
    rs.width = pRevWidth->load();
    rs.depth = pRevDepth->load();
    reverb.process (buffer, rs);

    // 4) Stereo: mid/side width of the whole output
    if (buffer.getNumChannels() >= 2)
    {
        stereoSmoothed.setTargetValue (pStereo->load());

        float* l = buffer.getWritePointer (0);
        float* r = buffer.getWritePointer (1);

        for (int i = 0; i < numSamples; ++i)
        {
            const float a = stereoSmoothed.getNextValue();
            const float m = 0.5f * (l[i] + r[i]);
            const float s = 0.5f * (l[i] - r[i]) * a;
            l[i] = m + s;
            r[i] = m - s;
        }
    }

    outputScope.push (buffer);   // capture the final signal for the editor's Spread and Output visualisers
}

juce::AudioProcessorEditor* BlgAudioProcessor::createEditor()
{
    return new BlgAudioProcessorEditor (*this);
}

void BlgAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void BlgAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BlgAudioProcessor();
}
