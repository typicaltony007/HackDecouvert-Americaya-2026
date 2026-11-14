#pragma once

#include "../Parameters.h"
#include "CustomWaveTable.h"

#include <cmath>

//==============================================================================
/**
    A simple phase-accumulator oscillator.

    Saw and square use PolyBLEP correction to reduce aliasing, sine and triangle
    are computed directly, and "Custom" reads from a user-defined CustomWaveTable.
*/
class Oscillator
{
public:
    void setSampleRate (double newSampleRate) noexcept   { sampleRate = newSampleRate; }
    void resetPhase (double newPhase = 0.0) noexcept     { phase = newPhase; }

    float nextSample (Params::Waveform waveform, double frequencyHz, const CustomWaveTable& customTable) noexcept
    {
        const double increment = juce::jlimit (0.0, 0.5, frequencyHz / sampleRate);
        float out = 0.0f;

        switch (waveform)
        {
            case Params::Waveform::saw:
                out = (float) (2.0 * phase - 1.0 - polyBlep (phase, increment));
                break;

            case Params::Waveform::square:
            {
                double v = phase < 0.5 ? 1.0 : -1.0;
                v += polyBlep (phase, increment);
                v -= polyBlep (std::fmod (phase + 0.5, 1.0), increment);
                out = (float) v;
                break;
            }

            case Params::Waveform::sine:
                out = (float) std::sin (juce::MathConstants<double>::twoPi * phase);
                break;

            case Params::Waveform::triangle:
                // Naive triangle: its harmonics fall off at 1/n^2, so aliasing is already low.
                out = (float) (1.0 - 4.0 * std::abs (std::fmod (phase + 0.25, 1.0) - 0.5));
                break;

            case Params::Waveform::custom:
                out = customTable.getSample (phase);
                break;
        }

        phase += increment;
        if (phase >= 1.0)
            phase -= 1.0;

        return out;
    }

private:
    /** PolyBLEP residual: smooths the discontinuity of a naive waveform. */
    static double polyBlep (double t, double dt) noexcept
    {
        if (dt <= 0.0)
            return 0.0;

        if (t < dt)
        {
            t /= dt;
            return t + t - t * t - 1.0;
        }

        if (t > 1.0 - dt)
        {
            t = (t - 1.0) / dt;
            return t * t + t + t + 1.0;
        }

        return 0.0;
    }

    double sampleRate = 44100.0;
    double phase = 0.0;
};
