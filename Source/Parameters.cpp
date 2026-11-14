#include "Parameters.h"

namespace Params
{
namespace
{
    using Layout = juce::AudioProcessorValueTreeState::ParameterLayout;

    void addFloat (Layout& layout, const juce::String& id, const juce::String& name,
                   juce::NormalisableRange<float> range, float defaultValue, const juce::String& label = {})
    {
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, range, defaultValue,
                                                                 juce::AudioParameterFloatAttributes().withLabel (label)));
    }

    void addInt (Layout& layout, const juce::String& id, const juce::String& name, int min, int max, int defaultValue)
    {
        layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { id, 1 }, name, min, max, defaultValue));
    }

    void addChoice (Layout& layout, const juce::String& id, const juce::String& name,
                    const juce::StringArray& choices, int defaultIndex)
    {
        layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, choices, defaultIndex));
    }

    void addBool (Layout& layout, const juce::String& id, const juce::String& name, bool defaultValue)
    {
        layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, defaultValue));
    }

    juce::NormalisableRange<float> timeRangeMs()   { return { 0.5f, 10000.0f, 0.1f, 0.25f }; }
    juce::NormalisableRange<float> freqRange (float lo, float hi) { return { lo, hi, 1.0f, 0.25f }; }
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    Layout layout;

    addFloat (layout, ID::masterGain, "Master Gain", { -48.0f, 6.0f, 0.1f }, -6.0f, "dB");

    //==========================================================================
    // Oscillators
    for (int i = 0; i < numOscillators; ++i)
    {
        const juce::String prefix = i == 0 ? "Osc A " : "Osc B ";

        addChoice (layout, osc (i, "wave"),   prefix + "Waveform", waveformNames, (int) Waveform::sine);
        // Osc B starts silent so a new instance plays a plain sine wave.
        addFloat  (layout, osc (i, "level"),  prefix + "Level",    { 0.0f, 1.0f, 0.001f }, i == 0 ? 0.8f : 0.0f);
        addInt    (layout, osc (i, "octave"), prefix + "Octave",   -3, 3, 0);
        addInt    (layout, osc (i, "semi"),   prefix + "Semitone", -12, 12, 0);
        addFloat  (layout, osc (i, "fine"),   prefix + "Fine",     { -100.0f, 100.0f, 0.1f }, i == 0 ? 0.0f : 7.0f, "ct");
    }

    //==========================================================================
    // Modulation slots (envelope or LFO each)
    for (int i = 0; i < numModSlots; ++i)
    {
        const juce::String prefix = "Mod " + juce::String (i + 1) + " ";

        // Slot 1 defaults to the amp envelope so the synth is playable out of the box.
        const bool isAmpEnv   = (i == 0);
        const bool defaultLfo = (i % 2 == 1);

        addChoice (layout, mod (i, "mode"),   prefix + "Mode",        modModeNames, defaultLfo ? 1 : 0);
        addChoice (layout, mod (i, "dest"),   prefix + "Destination", modDestNames, isAmpEnv ? (int) ModDest::amp : (int) ModDest::none);
        addFloat  (layout, mod (i, "amount"), prefix + "Amount",      { -1.0f, 1.0f, 0.001f }, isAmpEnv ? 1.0f : 0.5f);

        addFloat  (layout, mod (i, "attack"),  prefix + "Attack",  timeRangeMs(), 5.0f,   "ms");
        addFloat  (layout, mod (i, "decay"),   prefix + "Decay",   timeRangeMs(), 250.0f, "ms");
        addFloat  (layout, mod (i, "sustain"), prefix + "Sustain", { 0.0f, 1.0f, 0.001f }, 0.8f);
        addFloat  (layout, mod (i, "release"), prefix + "Release", timeRangeMs(), 350.0f, "ms");

        addFloat  (layout, mod (i, "rate"),  prefix + "LFO Rate",  { 0.05f, 30.0f, 0.01f, 0.3f }, 4.0f, "Hz");
        addChoice (layout, mod (i, "shape"), prefix + "LFO Shape", lfoShapeNames, 0);
    }

    //==========================================================================
    // Effects
    addBool   (layout, ID::distOn,     "Distortion On", false);
    addChoice (layout, ID::distType,   "Distortion Type", distTypeNames, 0);
    addFloat  (layout, ID::distDrive,  "Distortion Drive",  { 0.0f, 40.0f, 0.1f }, 12.0f, "dB");
    addFloat  (layout, ID::distOutput, "Distortion Output", { -24.0f, 6.0f, 0.1f }, -6.0f, "dB");
    addFloat  (layout, ID::distMix,    "Distortion Mix",    { 0.0f, 1.0f, 0.001f }, 1.0f);

    addBool   (layout, ID::crushOn,    "Bitcrush On", false);
    addFloat  (layout, ID::crushBits,  "Bitcrush Bits",       { 1.0f, 16.0f, 0.01f }, 8.0f, "bit");
    addFloat  (layout, ID::crushRate,  "Bitcrush Downsample", { 1.0f, 50.0f, 1.0f, 0.5f }, 4.0f, "x");
    addFloat  (layout, ID::crushMix,   "Bitcrush Mix",        { 0.0f, 1.0f, 0.001f }, 1.0f);

    addBool   (layout, ID::eqOn,       "EQ On", false);
    addFloat  (layout, ID::eqLowFreq,  "EQ Low Freq",  freqRange (20.0f, 1000.0f),     120.0f, "Hz");
    addFloat  (layout, ID::eqLowGain,  "EQ Low Gain",  { -18.0f, 18.0f, 0.1f }, 0.0f, "dB");
    addFloat  (layout, ID::eqMidFreq,  "EQ Mid Freq",  freqRange (100.0f, 10000.0f),   1000.0f, "Hz");
    addFloat  (layout, ID::eqMidGain,  "EQ Mid Gain",  { -18.0f, 18.0f, 0.1f }, 0.0f, "dB");
    addFloat  (layout, ID::eqMidQ,     "EQ Mid Q",     { 0.1f, 10.0f, 0.01f, 0.4f }, 0.707f);
    addFloat  (layout, ID::eqHighFreq, "EQ High Freq", freqRange (1000.0f, 20000.0f),  8000.0f, "Hz");
    addFloat  (layout, ID::eqHighGain, "EQ High Gain", { -18.0f, 18.0f, 0.1f }, 0.0f, "dB");

    addBool   (layout, ID::delayOn,       "Delay On", false);
    addFloat  (layout, ID::delayTime,     "Delay Time",     { 1.0f, 2000.0f, 0.1f, 0.4f }, 350.0f, "ms");
    addFloat  (layout, ID::delayFeedback, "Delay Feedback", { 0.0f, 0.95f, 0.001f }, 0.35f);
    addFloat  (layout, ID::delayMix,      "Delay Mix",      { 0.0f, 1.0f, 0.001f }, 0.3f);

    addBool   (layout, ID::reverbOn,      "Reverb On", false);
    addFloat  (layout, ID::reverbSize,    "Reverb Size",    { 0.0f, 1.0f, 0.001f }, 0.6f);
    addFloat  (layout, ID::reverbDamping, "Reverb Damping", { 0.0f, 1.0f, 0.001f }, 0.5f);
    addFloat  (layout, ID::reverbWidth,   "Reverb Width",   { 0.0f, 1.0f, 0.001f }, 1.0f);
    addFloat  (layout, ID::reverbMix,     "Reverb Mix",     { 0.0f, 1.0f, 0.001f }, 0.25f);

    addBool   (layout, ID::filterOn,        "Filter On", false);
    addChoice (layout, ID::filterType,      "Filter Type", filterTypeNames, 0);
    addFloat  (layout, ID::filterCutoff,    "Filter Cutoff",    freqRange (20.0f, 20000.0f), 2000.0f, "Hz");
    addFloat  (layout, ID::filterResonance, "Filter Resonance", { 0.3f, 10.0f, 0.01f, 0.4f }, 0.707f);
    addFloat  (layout, ID::filterMix,       "Filter Mix",       { 0.0f, 1.0f, 0.001f }, 1.0f);

    addBool   (layout, ID::chorusOn,       "Chorus On", false);
    addFloat  (layout, ID::chorusRate,     "Chorus Rate",     { 0.05f, 10.0f, 0.01f, 0.4f }, 1.0f, "Hz");
    addFloat  (layout, ID::chorusDepth,    "Chorus Depth",    { 0.0f, 1.0f, 0.001f }, 0.3f);
    addFloat  (layout, ID::chorusDelay,    "Chorus Delay",    { 1.0f, 30.0f, 0.1f }, 7.0f, "ms");
    addFloat  (layout, ID::chorusFeedback, "Chorus Feedback", { -0.95f, 0.95f, 0.001f }, 0.0f);
    addFloat  (layout, ID::chorusMix,      "Chorus Mix",      { 0.0f, 1.0f, 0.001f }, 0.5f);

    addBool   (layout, ID::compOn,        "Compressor On", false);
    addFloat  (layout, ID::compThreshold, "Compressor Threshold", { -60.0f, 0.0f, 0.1f }, -18.0f, "dB");
    addFloat  (layout, ID::compRatio,     "Compressor Ratio",     { 1.0f, 20.0f, 0.01f, 0.5f }, 4.0f, ":1");
    addFloat  (layout, ID::compAttack,    "Compressor Attack",    { 0.1f, 200.0f, 0.1f, 0.4f }, 10.0f, "ms");
    addFloat  (layout, ID::compRelease,   "Compressor Release",   { 10.0f, 1000.0f, 1.0f, 0.4f }, 120.0f, "ms");
    addFloat  (layout, ID::compMakeup,    "Compressor Makeup",    { 0.0f, 24.0f, 0.1f }, 0.0f, "dB");

    return layout;
}
}
