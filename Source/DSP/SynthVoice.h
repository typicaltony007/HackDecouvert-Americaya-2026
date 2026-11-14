#pragma once

#include "../Parameters.h"
#include "CustomWaveTable.h"
#include "ModSource.h"
#include "Oscillator.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>

//==============================================================================
/** Parameter snapshot shared by all voices; refreshed once per audio block. */
struct VoiceParams
{
    struct Osc
    {
        Params::Waveform wave = Params::Waveform::sine;
        float level = 0.8f;
        float pitchOffsetSemis = 0.0f;   // octave*12 + semi + fine/100
    };

    std::array<Osc, Params::numOscillators> osc;
    std::array<ModSlotSettings, Params::numModSlots> mods;
};

using CustomTables = std::array<CustomWaveTable, Params::numOscillators>;

//==============================================================================
/** Every voice plays the same sound, so this just accepts every note. */
struct SynthSound final : public juce::SynthesiserSound
{
    bool appliesToNote (int) override    { return true; }
    bool appliesToChannel (int) override { return true; }
};

//==============================================================================
/**
    One polyphonic voice: two oscillators (A + B) and five modulation sources
    (envelopes / LFOs) that can be routed to amp, pitch, level or pan.
*/
class SynthVoice final : public juce::SynthesiserVoice
{
public:
    SynthVoice (const VoiceParams& sharedParams, const CustomTables& customTables);

    void prepare (double sampleRate);

    bool canPlaySound (juce::SynthesiserSound* sound) override;
    void startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*, int currentPitchWheelPosition) override;
    void stopNote (float velocity, bool allowTailOff) override;
    void pitchWheelMoved (int newPitchWheelValue) override;
    void controllerMoved (int, int) override {}

    void renderNextBlock (juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override;
    using juce::SynthesiserVoice::renderNextBlock;

private:
    void updateModSettings();
    bool ampEnvelopeRouted() const noexcept;
    void finishNote();

    static float pitchWheelToSemis (int wheelValue) noexcept
    {
        return (float) (wheelValue - 8192) / 8192.0f * pitchBendRangeSemis;
    }

    static constexpr float pitchBendRangeSemis = 2.0f;
    static constexpr float modPitchRangeSemis  = 24.0f;
    static constexpr float voiceGain           = 0.3f;

    const VoiceParams& params;
    const CustomTables& tables;

    std::array<Oscillator, Params::numOscillators> oscillators;
    std::array<ModSource, Params::numModSlots> modSources;

    /** Fallback amp gate, used only when no envelope is routed to "Amp". */
    juce::ADSR gate;

    double noteFrequency = 440.0;
    float velocityGain = 1.0f;
    float pitchBendSemis = 0.0f;
    bool noteReleased = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SynthVoice)
};
