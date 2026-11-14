#pragma once

#include "../Parameters.h"

#include <juce_dsp/juce_dsp.h>

//==============================================================================
/**
    Stereo effects processed in a user-defined order (the node chain).

    The order comes from the processor each block as a list of Params::Effect
    indices. An effect is processed if it's in the chain and its "On" switch
    is enabled. An effect's state (delay lines, reverb tails...) is cleared
    when it becomes active again, so stale audio doesn't replay.
*/
class EffectsChain
{
public:
    using Order = std::array<int, Params::numEffects>;

    explicit EffectsChain (juce::AudioProcessorValueTreeState& apvts);

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();

    /** Processes the effects listed in order[0 .. count-1]. */
    void process (juce::AudioBuffer<float>& buffer, const Order& order, int count);

private:
    using Effect = Params::Effect;

    void processEffect (Effect, juce::AudioBuffer<float>&);
    void resetEffect (Effect);

    void processDistortion (juce::AudioBuffer<float>&);
    void processBitcrush   (juce::AudioBuffer<float>&);
    void processFilter     (juce::AudioBuffer<float>&);
    void processEq         (juce::AudioBuffer<float>&);
    void processChorus     (juce::AudioBuffer<float>&);
    void processCompressor (juce::AudioBuffer<float>&);
    void processDelay      (juce::AudioBuffer<float>&);
    void processReverb     (juce::AudioBuffer<float>&);

    void updateEqCoefficients();
    static float distort (Params::DistType type, float x) noexcept;

    //==========================================================================
    /** Raw parameter pointers, looked up once so the audio thread never searches by ID. */
    struct Raw
    {
        using P = std::atomic<float>*;
        P distType {}, distDrive {}, distOutput {}, distMix {};
        P crushBits {}, crushRate {}, crushMix {};
        P filterType {}, filterCutoff {}, filterResonance {}, filterMix {};
        P eqLowFreq {}, eqLowGain {}, eqMidFreq {}, eqMidGain {}, eqMidQ {}, eqHighFreq {}, eqHighGain {};
        P chorusRate {}, chorusDepth {}, chorusDelay {}, chorusFeedback {}, chorusMix {};
        P compThreshold {}, compRatio {}, compAttack {}, compRelease {}, compMakeup {};
        P delayTime {}, delayFeedback {}, delayMix {};
        P reverbSize {}, reverbDamping {}, reverbWidth {}, reverbMix {};
    } raw;

    std::array<std::atomic<float>*, Params::numEffects> onParams {};
    std::array<bool, Params::numEffects> wasActive {};

    double sampleRate = 44100.0;

    // Bitcrush
    std::array<float, 2> crushHeld { 0.0f, 0.0f };
    int crushCounter = 0;

    // Filter + a dry copy for its mix control
    juce::dsp::StateVariableTPTFilter<float> filter;
    juce::AudioBuffer<float> dryBuffer;

    // EQ
    using StereoFilter = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>>;
    StereoFilter lowShelf, midPeak, highShelf;
    std::array<float, 7> lastEqValues {};

    // Chorus / compressor
    juce::dsp::Chorus<float> chorus;
    juce::dsp::Compressor<float> compressor;

    // Delay
    static constexpr double maxDelaySeconds = 2.5;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLine;
    juce::SmoothedValue<float> delayTimeSamples;

    // Reverb
    juce::dsp::Reverb reverb;
};
