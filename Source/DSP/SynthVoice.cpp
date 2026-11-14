#include "SynthVoice.h"

#include <cmath>

using Params::ModDest;

SynthVoice::SynthVoice (const VoiceParams& sharedParams, const CustomTables& customTables)
    : params (sharedParams), tables (customTables)
{
}

void SynthVoice::prepare (double sampleRate)
{
    setCurrentPlaybackSampleRate (sampleRate);

    for (auto& o : oscillators) o.setSampleRate (sampleRate);
    for (auto& m : modSources)  m.setSampleRate (sampleRate);

    // ADSR only recalculates its rates in setParameters(), so set the sample rate first.
    gate.setSampleRate (sampleRate);
    gate.setParameters ({ 0.002f, 0.0f, 1.0f, 0.03f });
}

bool SynthVoice::canPlaySound (juce::SynthesiserSound* sound)
{
    return dynamic_cast<SynthSound*> (sound) != nullptr;
}

void SynthVoice::startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*, int currentPitchWheelPosition)
{
    noteFrequency  = juce::MidiMessage::getMidiNoteInHertz (midiNoteNumber);
    velocityGain   = velocity;
    pitchBendSemis = pitchWheelToSemis (currentPitchWheelPosition);
    noteReleased   = false;

    for (auto& o : oscillators)
        o.resetPhase();

    updateModSettings();

    for (auto& m : modSources)
        m.noteOn();

    gate.noteOn();
}

void SynthVoice::stopNote (float, bool allowTailOff)
{
    if (allowTailOff)
    {
        noteReleased = true;

        for (auto& m : modSources)
            m.noteOff();

        gate.noteOff();
    }
    else
    {
        finishNote();
    }
}

void SynthVoice::pitchWheelMoved (int newPitchWheelValue)
{
    pitchBendSemis = pitchWheelToSemis (newPitchWheelValue);
}

void SynthVoice::updateModSettings()
{
    for (size_t i = 0; i < modSources.size(); ++i)
        modSources[i].applySettings (params.mods[i]);
}

bool SynthVoice::ampEnvelopeRouted() const noexcept
{
    for (auto& m : modSources)
        if (m.isEnvelope() && m.getSettings().dest == ModDest::amp)
            return true;

    return false;
}

void SynthVoice::finishNote()
{
    clearCurrentNote();

    for (auto& m : modSources)
        m.reset();

    gate.reset();
    noteReleased = false;
}

void SynthVoice::renderNextBlock (juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples)
{
    if (! isVoiceActive())
        return;

    updateModSettings();
    const bool useAmpEnvelopes = ampEnvelopeRouted();

    const int numChannels = outputBuffer.getNumChannels();
    auto* left  = outputBuffer.getWritePointer (0);
    auto* right = numChannels > 1 ? outputBuffer.getWritePointer (1) : nullptr;

    for (int s = startSample; s < startSample + numSamples; ++s)
    {
        //======================================================================
        // 1. Gather modulation for this sample
        float pitchMod = 0.0f;
        std::array<float, Params::numOscillators> oscPitchMod { 0.0f, 0.0f };
        std::array<float, Params::numOscillators> levelMod    { 0.0f, 0.0f };
        float ampGain = 1.0f;
        float pan = 0.0f;
        bool anyAmpEnvelopeActive = false;

        for (auto& m : modSources)
        {
            const float value = m.next();
            const auto& set = m.getSettings();
            const float amt = set.amount;

            switch (set.dest)
            {
                case ModDest::none:      break;
                case ModDest::pitch:     pitchMod       += amt * value * modPitchRangeSemis; break;
                case ModDest::oscAPitch: oscPitchMod[0] += amt * value * modPitchRangeSemis; break;
                case ModDest::oscBPitch: oscPitchMod[1] += amt * value * modPitchRangeSemis; break;
                case ModDest::oscALevel: levelMod[0]    += amt * value; break;
                case ModDest::oscBLevel: levelMod[1]    += amt * value; break;
                case ModDest::pan:       pan            += amt * value; break;

                case ModDest::amp:
                    if (m.isEnvelope())
                    {
                        // amount 1 -> gain follows envelope; amount -1 -> inverted envelope
                        ampGain *= amt >= 0.0f ? (1.0f - amt + amt * value) : (1.0f + amt * value);
                        anyAmpEnvelopeActive = anyAmpEnvelopeActive || m.isEnvelopeActive();
                    }
                    else
                    {
                        // LFO tremolo: dips the gain by up to |amount|
                        ampGain *= 1.0f - std::abs (amt) * 0.5f * (1.0f - value);
                    }
                    break;
            }
        }

        const float gateValue = gate.getNextSample();
        if (! useAmpEnvelopes)
            ampGain *= gateValue;

        //======================================================================
        // 2. Oscillators
        float mono = 0.0f;

        for (size_t i = 0; i < oscillators.size(); ++i)
        {
            const auto& oscParams = params.osc[i];
            const float level = juce::jlimit (0.0f, 1.0f, oscParams.level + levelMod[i]);

            const float semis = oscParams.pitchOffsetSemis + pitchBendSemis + pitchMod + oscPitchMod[i];
            const double freq = noteFrequency * std::exp2 ((double) semis / 12.0);

            const float sample = oscillators[i].nextSample (oscParams.wave, freq, tables[i]);
            mono += sample * level;
        }

        mono *= ampGain * velocityGain * voiceGain;

        //======================================================================
        // 3. Equal-power pan and output
        if (right != nullptr)
        {
            const float angle = (juce::jlimit (-1.0f, 1.0f, pan) + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
            left[s]  += mono * std::cos (angle) * juce::MathConstants<float>::sqrt2;
            right[s] += mono * std::sin (angle) * juce::MathConstants<float>::sqrt2;
        }
        else
        {
            left[s] += mono;
        }

        //======================================================================
        // 4. End the voice once its amplitude envelope has finished
        if (noteReleased)
        {
            const bool finished = useAmpEnvelopes ? ! anyAmpEnvelopeActive : ! gate.isActive();

            if (finished)
            {
                finishNote();
                break;
            }
        }
    }
}
