#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace Params;

//==============================================================================
SynthAudioProcessor::SynthAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createLayout()),
      effects (apvts)
{
    auto get = [this] (const juce::String& id)
    {
        auto* p = apvts.getRawParameterValue (id);
        jassert (p != nullptr);   // unknown parameter ID
        return p;
    };

    for (int i = 0; i < numOscillators; ++i)
    {
        auto& o = oscRaw[(size_t) i];
        o.wave   = get (osc (i, "wave"));
        o.level  = get (osc (i, "level"));
        o.octave = get (osc (i, "octave"));
        o.semi   = get (osc (i, "semi"));
        o.fine   = get (osc (i, "fine"));
    }

    for (int i = 0; i < numModSlots; ++i)
    {
        auto& m = modRaw[(size_t) i];
        m.mode    = get (mod (i, "mode"));
        m.dest    = get (mod (i, "dest"));
        m.amount  = get (mod (i, "amount"));
        m.attack  = get (mod (i, "attack"));
        m.decay   = get (mod (i, "decay"));
        m.sustain = get (mod (i, "sustain"));
        m.release = get (mod (i, "release"));
        m.rate    = get (mod (i, "rate"));
        m.shape   = get (mod (i, "shape"));
    }

    masterGainRaw = get (ID::masterGain);

    // Non-parameter state lives in the APVTS tree so it's saved with the session:
    // the custom-wave formulas and the effect chain (empty = plain synth sound).
    for (int i = 0; i < numOscillators; ++i)
        apvts.state.setProperty (customFormulaProperty (i), defaultCustomFormula, nullptr);

    apvts.state.setProperty (fxChainProperty, juce::String(), nullptr);

    rebuildCustomTablesFromState();
    publishFxChainFromState();
    updateVoiceParams();

    synth.addSound (new SynthSound());

    for (int i = 0; i < numVoices; ++i)
        synth.addVoice (new SynthVoice (voiceParams, customTables));
}

//==============================================================================
void SynthAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);

    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* voice = dynamic_cast<SynthVoice*> (synth.getVoice (i)))
            voice->prepare (sampleRate);

    const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock,
                                        (juce::uint32) juce::jmax (1, getTotalNumOutputChannels()) };
    effects.prepare (spec);

    masterGain.reset (sampleRate, 0.02);
    masterGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (masterGainRaw->load()));
}

void SynthAudioProcessor::releaseResources()
{
    keyboardState.reset();
}

bool SynthAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Synth: no inputs, stereo output only.
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainInputChannelSet().isDisabled();
}

void SynthAudioProcessor::updateVoiceParams()
{
    for (size_t i = 0; i < (size_t) numOscillators; ++i)
    {
        auto& o = voiceParams.osc[i];
        const auto& r = oscRaw[i];

        o.wave  = (Waveform) juce::roundToInt (r.wave->load());
        o.level = r.level->load();
        o.pitchOffsetSemis = r.octave->load() * 12.0f + r.semi->load() + r.fine->load() * 0.01f;
    }

    for (size_t i = 0; i < (size_t) numModSlots; ++i)
    {
        auto& m = voiceParams.mods[i];
        const auto& r = modRaw[i];

        m.mode   = (ModMode)  juce::roundToInt (r.mode->load());
        m.dest   = (ModDest)  juce::roundToInt (r.dest->load());
        m.shape  = (LfoShape) juce::roundToInt (r.shape->load());
        m.amount = r.amount->load();
        m.rateHz = r.rate->load();
        m.adsr   = { r.attack->load() * 0.001f, r.decay->load() * 0.001f,
                     r.sustain->load(),         r.release->load() * 0.001f };
    }
}

void SynthAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    buffer.clear();

    // Merge notes from the on-screen and computer keyboards into the host MIDI.
    keyboardState.processNextMidiBuffer (midiMessages, 0, numSamples, true);

    // Audio in: the synth voices
    updateVoiceParams();
    synth.renderNextBlock (buffer, midiMessages, 0, numSamples);

    // Effect nodes, upstream to downstream
    EffectsChain::Order order {};
    const int count = juce::jlimit (0, numEffects, fxCount.load (std::memory_order_acquire));

    for (int i = 0; i < count; ++i)
        order[(size_t) i] = fxOrder[(size_t) i].load (std::memory_order_relaxed);

    effects.process (buffer, order, count);

    // Audio out
    masterGain.setTargetValue (juce::Decibels::decibelsToGain (masterGainRaw->load()));

    if (masterGain.isSmoothing())
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const float g = masterGain.getNextValue();
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                buffer.getWritePointer (ch)[i] *= g;
        }
    }
    else
    {
        buffer.applyGain (masterGain.getTargetValue());
    }

    outputTap.push (buffer);
}

//==============================================================================
juce::String SynthAudioProcessor::setCustomFormula (int oscIndex, const juce::String& formula)
{
    jassert (juce::isPositiveAndBelow (oscIndex, numOscillators));

    const auto error = customTables[(size_t) oscIndex].rebuild (formula);

    if (error.isEmpty())
        apvts.state.setProperty (customFormulaProperty (oscIndex), formula, nullptr);

    return error;
}

juce::String SynthAudioProcessor::getCustomFormula (int oscIndex) const
{
    return apvts.state.getProperty (customFormulaProperty (oscIndex), defaultCustomFormula).toString();
}

void SynthAudioProcessor::rebuildCustomTablesFromState()
{
    for (int i = 0; i < numOscillators; ++i)
    {
        const auto error = customTables[(size_t) i].rebuild (getCustomFormula (i));

        if (error.isNotEmpty())   // a bad saved formula falls back to the default
            customTables[(size_t) i].rebuild (defaultCustomFormula);
    }
}

//==============================================================================
std::vector<Effect> SynthAudioProcessor::getFxChain() const
{
    std::vector<Effect> chain;
    const auto keys = juce::StringArray::fromTokens (apvts.state.getProperty (fxChainProperty).toString(), ",", "");

    for (const auto& key : keys)
    {
        for (int i = 0; i < numEffects; ++i)
        {
            const auto effect = (Effect) i;

            if (key.trim() == effectInfo[(size_t) i].key
                && std::find (chain.begin(), chain.end(), effect) == chain.end())
            {
                chain.push_back (effect);
            }
        }
    }

    return chain;
}

void SynthAudioProcessor::setFxChain (const std::vector<Effect>& chain)
{
    juce::StringArray keys;

    for (auto effect : chain)
        keys.add (effectInfo[(size_t) effect].key);

    apvts.state.setProperty (fxChainProperty, keys.joinIntoString (","), nullptr);
    publishFxChainFromState();
}

void SynthAudioProcessor::publishFxChainFromState()
{
    const auto chain = getFxChain();

    // Hide the old list from the audio thread while it's being rewritten.
    // (A block that started just before this may still see a mix of old and
    // new entries; EffectsChain ignores duplicates, so that's harmless.)
    fxCount.store (0, std::memory_order_release);

    for (size_t i = 0; i < chain.size(); ++i)
        fxOrder[i].store ((int) chain[i], std::memory_order_relaxed);

    fxCount.store ((int) chain.size(), std::memory_order_release);
}

//==============================================================================
void SynthAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void SynthAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));

            // Sessions saved before the node chain existed: rebuild the chain from
            // whichever effects were switched on, in the old fixed order.
            if (! apvts.state.hasProperty (fxChainProperty))
            {
                std::vector<Effect> chain;

                for (auto effect : { Effect::distortion, Effect::bitcrush, Effect::eq, Effect::delay, Effect::reverb })
                    if (auto* on = apvts.getRawParameterValue (effectInfo[(size_t) effect].onParamID); on != nullptr && on->load() >= 0.5f)
                        chain.push_back (effect);

                setFxChain (chain);
            }

            rebuildCustomTablesFromState();
            publishFxChainFromState();
        }
    }
}

//==============================================================================
juce::AudioProcessorEditor* SynthAudioProcessor::createEditor()
{
    return new SynthAudioProcessorEditor (*this);
}

// This creates new instances of the plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SynthAudioProcessor();
}
