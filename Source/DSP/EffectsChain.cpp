#include "EffectsChain.h"

#include <cmath>

using namespace Params;

EffectsChain::EffectsChain (juce::AudioProcessorValueTreeState& apvts)
{
    auto get = [&apvts] (const char* id)
    {
        auto* p = apvts.getRawParameterValue (id);
        jassert (p != nullptr);   // unknown parameter ID
        return p;
    };

    for (int i = 0; i < numEffects; ++i)
        onParams[(size_t) i] = get (effectInfo[(size_t) i].onParamID);

    raw.distType        = get (ID::distType);
    raw.distDrive       = get (ID::distDrive);
    raw.distOutput      = get (ID::distOutput);
    raw.distMix         = get (ID::distMix);

    raw.crushBits       = get (ID::crushBits);
    raw.crushRate       = get (ID::crushRate);
    raw.crushMix        = get (ID::crushMix);

    raw.filterType      = get (ID::filterType);
    raw.filterCutoff    = get (ID::filterCutoff);
    raw.filterResonance = get (ID::filterResonance);
    raw.filterMix       = get (ID::filterMix);

    raw.eqLowFreq       = get (ID::eqLowFreq);
    raw.eqLowGain       = get (ID::eqLowGain);
    raw.eqMidFreq       = get (ID::eqMidFreq);
    raw.eqMidGain       = get (ID::eqMidGain);
    raw.eqMidQ          = get (ID::eqMidQ);
    raw.eqHighFreq      = get (ID::eqHighFreq);
    raw.eqHighGain      = get (ID::eqHighGain);

    raw.chorusRate      = get (ID::chorusRate);
    raw.chorusDepth     = get (ID::chorusDepth);
    raw.chorusDelay     = get (ID::chorusDelay);
    raw.chorusFeedback  = get (ID::chorusFeedback);
    raw.chorusMix       = get (ID::chorusMix);

    raw.compThreshold   = get (ID::compThreshold);
    raw.compRatio       = get (ID::compRatio);
    raw.compAttack      = get (ID::compAttack);
    raw.compRelease     = get (ID::compRelease);
    raw.compMakeup      = get (ID::compMakeup);

    raw.delayTime       = get (ID::delayTime);
    raw.delayFeedback   = get (ID::delayFeedback);
    raw.delayMix        = get (ID::delayMix);

    raw.reverbSize      = get (ID::reverbSize);
    raw.reverbDamping   = get (ID::reverbDamping);
    raw.reverbWidth     = get (ID::reverbWidth);
    raw.reverbMix       = get (ID::reverbMix);
}

void EffectsChain::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;

    filter.prepare (spec);
    dryBuffer.setSize ((int) spec.numChannels, (int) spec.maximumBlockSize);

    // EQ: allocate coefficient objects here (not on the audio thread)
    lowShelf.state  = juce::dsp::IIR::Coefficients<float>::makeLowShelf   (sampleRate, 120.0f, 0.707f, 1.0f);
    midPeak.state   = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sampleRate, 1000.0f, 0.707f, 1.0f);
    highShelf.state = juce::dsp::IIR::Coefficients<float>::makeHighShelf  (sampleRate, 8000.0f, 0.707f, 1.0f);
    lowShelf.prepare (spec);
    midPeak.prepare (spec);
    highShelf.prepare (spec);
    lastEqValues.fill (-1.0f);   // force a coefficient update on the first block

    chorus.prepare (spec);
    compressor.prepare (spec);

    delayLine.setMaximumDelayInSamples ((int) std::ceil (maxDelaySeconds * sampleRate));
    delayLine.prepare (spec);
    delayTimeSamples.reset (sampleRate, 0.05);
    delayTimeSamples.setCurrentAndTargetValue (raw.delayTime->load() * 0.001f * (float) sampleRate);

    reverb.prepare (spec);

    reset();
}

void EffectsChain::reset()
{
    for (int i = 0; i < numEffects; ++i)
        resetEffect ((Effect) i);

    wasActive.fill (false);
}

void EffectsChain::resetEffect (Effect effect)
{
    switch (effect)
    {
        case Effect::distortion: break;
        case Effect::bitcrush:   crushHeld = { 0.0f, 0.0f }; crushCounter = 0; break;
        case Effect::filter:     filter.reset(); break;
        case Effect::eq:         lowShelf.reset(); midPeak.reset(); highShelf.reset(); break;
        case Effect::chorus:     chorus.reset(); break;
        case Effect::compressor: compressor.reset(); break;
        case Effect::delay:      delayLine.reset(); break;
        case Effect::reverb:     reverb.reset(); break;
        case Effect::count:      break;
    }
}

void EffectsChain::process (juce::AudioBuffer<float>& buffer, const Order& order, int count)
{
    std::array<bool, numEffects> activeNow {};

    for (int i = 0; i < juce::jmin (count, numEffects); ++i)
    {
        const int index = order[(size_t) i];

        // Skip invalid entries, duplicates and bypassed effects
        if (! juce::isPositiveAndBelow (index, numEffects) || activeNow[(size_t) index])
            continue;

        if (onParams[(size_t) index]->load() < 0.5f)
            continue;

        activeNow[(size_t) index] = true;

        if (! wasActive[(size_t) index])
            resetEffect ((Effect) index);

        processEffect ((Effect) index, buffer);
    }

    wasActive = activeNow;
}

void EffectsChain::processEffect (Effect effect, juce::AudioBuffer<float>& buffer)
{
    switch (effect)
    {
        case Effect::distortion: processDistortion (buffer); break;
        case Effect::bitcrush:   processBitcrush (buffer);   break;
        case Effect::filter:     processFilter (buffer);     break;
        case Effect::eq:         processEq (buffer);         break;
        case Effect::chorus:     processChorus (buffer);     break;
        case Effect::compressor: processCompressor (buffer); break;
        case Effect::delay:      processDelay (buffer);      break;
        case Effect::reverb:     processReverb (buffer);     break;
        case Effect::count:      break;
    }
}

//==============================================================================
float EffectsChain::distort (DistType type, float x) noexcept
{
    switch (type)
    {
        case DistType::softClip:  return std::tanh (x);
        case DistType::overdrive: return 2.0f / juce::MathConstants<float>::pi * std::atan (x);
        case DistType::hardClip:  return juce::jlimit (-1.0f, 1.0f, x);
        case DistType::tube:      return std::tanh (x + 0.25f) - std::tanh (0.25f);   // asymmetric -> even harmonics
        case DistType::foldback:
        {
            // Triangle fold: reflects the signal back every time it crosses +/-1
            const float v = (x + 1.0f) * 0.25f;
            return 1.0f - 4.0f * std::abs (v - std::floor (v) - 0.5f);
        }
    }

    return x;
}

void EffectsChain::processDistortion (juce::AudioBuffer<float>& buffer)
{
    const auto type   = (DistType) juce::roundToInt (raw.distType->load());
    const float drive = juce::Decibels::decibelsToGain (raw.distDrive->load());
    const float out   = juce::Decibels::decibelsToGain (raw.distOutput->load());
    const float mix   = raw.distMix->load();

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* data = buffer.getWritePointer (ch);

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const float dry = data[i];
            const float wet = distort (type, dry * drive) * out;
            data[i] = dry + mix * (wet - dry);
        }
    }
}

void EffectsChain::processBitcrush (juce::AudioBuffer<float>& buffer)
{
    const float levels   = std::pow (2.0f, raw.crushBits->load() - 1.0f);
    const int downsample = juce::jmax (1, juce::roundToInt (raw.crushRate->load()));
    const float mix      = raw.crushMix->load();

    const int numChannels = juce::jmin (buffer.getNumChannels(), (int) crushHeld.size());

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const bool takeNewSample = (crushCounter == 0);
        crushCounter = (crushCounter + 1) % downsample;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = buffer.getWritePointer (ch);
            const float dry = data[i];

            if (takeNewSample)
                crushHeld[(size_t) ch] = std::round (dry * levels) / levels;

            data[i] = dry + mix * (crushHeld[(size_t) ch] - dry);
        }
    }
}

void EffectsChain::processFilter (juce::AudioBuffer<float>& buffer)
{
    using SvfType = juce::dsp::StateVariableTPTFilterType;

    switch ((FilterType) juce::roundToInt (raw.filterType->load()))
    {
        case FilterType::lowPass:  filter.setType (SvfType::lowpass);  break;
        case FilterType::highPass: filter.setType (SvfType::highpass); break;
        case FilterType::bandPass: filter.setType (SvfType::bandpass); break;
    }

    filter.setCutoffFrequency (juce::jmin (raw.filterCutoff->load(), (float) sampleRate * 0.45f));
    filter.setResonance (raw.filterResonance->load());

    const float mix = raw.filterMix->load();
    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    if (mix < 1.0f)
    {
        dryBuffer.setSize (numChannels, numSamples, false, false, true);
        for (int ch = 0; ch < numChannels; ++ch)
            dryBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);
    }

    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> context (block);
    filter.process (context);

    if (mix < 1.0f)
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            buffer.applyGain (ch, 0, numSamples, mix);
            buffer.addFrom (ch, 0, dryBuffer, ch, 0, numSamples, 1.0f - mix);
        }
    }
}

void EffectsChain::updateEqCoefficients()
{
    const std::array<float, 7> values { raw.eqLowFreq->load(),  raw.eqLowGain->load(),
                                        raw.eqMidFreq->load(),  raw.eqMidGain->load(), raw.eqMidQ->load(),
                                        raw.eqHighFreq->load(), raw.eqHighGain->load() };

    if (values == lastEqValues)
        return;

    lastEqValues = values;

    using Coeffs = juce::dsp::IIR::ArrayCoefficients<float>;
    const float nyquistSafe = (float) sampleRate * 0.45f;
    auto gain = [] (float dB) { return juce::Decibels::decibelsToGain (dB); };

    // Assigning a std::array into the existing Coefficients object avoids allocating on the audio thread.
    *lowShelf.state  = Coeffs::makeLowShelf   (sampleRate, juce::jmin (values[0], nyquistSafe), 0.707f, gain (values[1]));
    *midPeak.state   = Coeffs::makePeakFilter (sampleRate, juce::jmin (values[2], nyquistSafe), values[4], gain (values[3]));
    *highShelf.state = Coeffs::makeHighShelf  (sampleRate, juce::jmin (values[5], nyquistSafe), 0.707f, gain (values[6]));
}

void EffectsChain::processEq (juce::AudioBuffer<float>& buffer)
{
    updateEqCoefficients();

    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> context (block);
    lowShelf.process (context);
    midPeak.process (context);
    highShelf.process (context);
}

void EffectsChain::processChorus (juce::AudioBuffer<float>& buffer)
{
    chorus.setRate        (raw.chorusRate->load());
    chorus.setDepth       (raw.chorusDepth->load());
    chorus.setCentreDelay (raw.chorusDelay->load());
    chorus.setFeedback    (raw.chorusFeedback->load());
    chorus.setMix         (raw.chorusMix->load());

    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> context (block);
    chorus.process (context);
}

void EffectsChain::processCompressor (juce::AudioBuffer<float>& buffer)
{
    compressor.setThreshold (raw.compThreshold->load());
    compressor.setRatio     (juce::jmax (1.0f, raw.compRatio->load()));
    compressor.setAttack    (raw.compAttack->load());
    compressor.setRelease   (raw.compRelease->load());

    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> context (block);
    compressor.process (context);

    buffer.applyGain (juce::Decibels::decibelsToGain (raw.compMakeup->load()));
}

void EffectsChain::processDelay (juce::AudioBuffer<float>& buffer)
{
    const float maxSamples = (float) (maxDelaySeconds * sampleRate) - 2.0f;
    delayTimeSamples.setTargetValue (juce::jlimit (1.0f, maxSamples, raw.delayTime->load() * 0.001f * (float) sampleRate));

    const float feedback  = raw.delayFeedback->load();
    const float mix       = raw.delayMix->load();
    const int numChannels = buffer.getNumChannels();

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const float delaySamples = delayTimeSamples.getNextValue();

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = buffer.getWritePointer (ch);
            const float dry = data[i];
            const float delayed = delayLine.popSample (ch, delaySamples);

            delayLine.pushSample (ch, dry + delayed * feedback);
            data[i] = dry * (1.0f - mix) + delayed * mix;
        }
    }
}

void EffectsChain::processReverb (juce::AudioBuffer<float>& buffer)
{
    juce::Reverb::Parameters p;
    p.roomSize   = raw.reverbSize->load();
    p.damping    = raw.reverbDamping->load();
    p.width      = raw.reverbWidth->load();
    p.wetLevel   = raw.reverbMix->load();
    p.dryLevel   = 1.0f - p.wetLevel;
    p.freezeMode = 0.0f;
    reverb.setParameters (p);

    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> context (block);
    reverb.process (context);
}
