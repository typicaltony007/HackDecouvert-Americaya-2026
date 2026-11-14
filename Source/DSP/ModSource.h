#pragma once

#include "../Parameters.h"
#include <juce_audio_basics/juce_audio_basics.h>

//==============================================================================
/** Snapshot of one modulation slot's parameters, read once per block. */
struct ModSlotSettings
{
    Params::ModMode  mode   = Params::ModMode::envelope;
    Params::ModDest  dest   = Params::ModDest::none;
    float            amount = 0.0f;

    juce::ADSR::Parameters adsr { 0.005f, 0.25f, 0.8f, 0.35f };   // seconds / level

    float            rateHz = 4.0f;
    Params::LfoShape shape  = Params::LfoShape::sine;
};

//==============================================================================
/**
    One per-voice modulation source: either an ADSR envelope (output 0..1)
    or an LFO (output -1..1). Both run all the time so switching modes while
    a note is held behaves predictably.
*/
class ModSource
{
public:
    void setSampleRate (double sr)
    {
        sampleRate = sr;
        envelope.setSampleRate (sr);
    }

    void applySettings (const ModSlotSettings& s)
    {
        settings = s;
        envelope.setParameters (s.adsr);
    }

    void noteOn()
    {
        envelope.noteOn();
        lfoPhase = 0.0;
        heldRandom = random.nextFloat() * 2.0f - 1.0f;
    }

    void noteOff()      { envelope.noteOff(); }
    void reset()        { envelope.reset(); lfoPhase = 0.0; }

    bool isEnvelope() const noexcept          { return settings.mode == Params::ModMode::envelope; }
    bool isEnvelopeActive() const noexcept    { return envelope.isActive(); }
    const ModSlotSettings& getSettings() const noexcept { return settings; }

    /** Advances by one sample and returns the current modulation value. */
    float next() noexcept
    {
        const float env = envelope.getNextSample();
        const float lfo = nextLfo();
        return isEnvelope() ? env : lfo;
    }

private:
    float nextLfo() noexcept
    {
        const double p = lfoPhase;
        float out = 0.0f;

        switch (settings.shape)
        {
            case Params::LfoShape::sine:          out = (float) std::sin (juce::MathConstants<double>::twoPi * p); break;
            case Params::LfoShape::triangle:      out = (float) (1.0 - 4.0 * std::abs (p - 0.5)); break;
            case Params::LfoShape::saw:           out = (float) (2.0 * p - 1.0); break;
            case Params::LfoShape::square:        out = p < 0.5 ? 1.0f : -1.0f; break;
            case Params::LfoShape::sampleAndHold: out = heldRandom; break;
        }

        lfoPhase += settings.rateHz / sampleRate;
        if (lfoPhase >= 1.0)
        {
            lfoPhase -= 1.0;
            heldRandom = random.nextFloat() * 2.0f - 1.0f;
        }

        return out;
    }

    ModSlotSettings settings;
    juce::ADSR envelope;
    juce::Random random;
    double sampleRate = 44100.0;
    double lfoPhase = 0.0;
    float heldRandom = 0.0f;
};
