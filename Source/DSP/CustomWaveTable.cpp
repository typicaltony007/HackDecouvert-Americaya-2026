#include "CustomWaveTable.h"

#include <cmath>

namespace
{
    /** Expression scope providing the variable x, constants and extra math functions. */
    class WaveScope final : public juce::Expression::Scope
    {
    public:
        double x = 0.0;
        mutable juce::Random random { 12345 };

        juce::Expression getSymbolValue (const juce::String& symbol) const override
        {
            if (symbol == "x")   return juce::Expression (x);
            if (symbol == "pi")  return juce::Expression (juce::MathConstants<double>::pi);
            if (symbol == "e")   return juce::Expression (juce::MathConstants<double>::euler);

            return Scope::getSymbolValue (symbol);   // throws "Unknown symbol"
        }

        double evaluateFunction (const juce::String& name, const double* p, int numParams) const override
        {
            constexpr double twoPi = juce::MathConstants<double>::twoPi;

            auto frac = [] (double v) { return v - std::floor (v); };

            if (numParams == 0 && name == "noise")
                return random.nextDouble() * 2.0 - 1.0;

            if (numParams == 1)
            {
                const double v = p[0];

                if (name == "sqrt")   return std::sqrt (std::abs (v));
                if (name == "exp")    return std::exp (v);
                if (name == "log")    return v > 0.0 ? std::log (v) : 0.0;
                if (name == "tanh")   return std::tanh (v);
                if (name == "sign")   return v > 0.0 ? 1.0 : (v < 0.0 ? -1.0 : 0.0);
                if (name == "floor")  return std::floor (v);
                if (name == "frac")   return frac (v);

                // Naive band-unlimited shapes with period 2*pi, handy for building formulas
                if (name == "saw")    return 2.0 * frac (v / twoPi) - 1.0;
                if (name == "square") return frac (v / twoPi) < 0.5 ? 1.0 : -1.0;
                if (name == "tri")    return 1.0 - 4.0 * std::abs (frac (v / twoPi + 0.25) - 0.5);
            }

            if (numParams == 2 && name == "pow")
                return std::pow (std::abs (p[0]), p[1]) * (p[0] < 0.0 ? -1.0 : 1.0);

            return Scope::evaluateFunction (name, p, numParams);   // sin cos tan abs min max
        }
    };
}

CustomWaveTable::CustomWaveTable()
{
    // Start with a plain sine so the oscillator is valid before any formula is applied.
    for (size_t i = 0; i < (size_t) tableSize; ++i)
        tables[0][i] = (float) std::sin (juce::MathConstants<double>::twoPi * (double) i / tableSize);
}

juce::String CustomWaveTable::rebuild (const juce::String& formula)
{
    if (formula.trim().isEmpty())
        return "Formula is empty";

    juce::String parseError;
    juce::Expression expression (formula, parseError);

    if (parseError.isNotEmpty())
        return parseError;

    WaveScope scope;
    std::array<double, tableSize> values {};

    for (size_t i = 0; i < (size_t) tableSize; ++i)
    {
        scope.x = juce::MathConstants<double>::twoPi * (double) i / tableSize;

        juce::String evalError;
        const double v = expression.evaluate (scope, evalError);

        if (evalError.isNotEmpty())
            return evalError;

        values[i] = std::isfinite (v) ? v : 0.0;
    }

    // Remove DC offset and normalise to +/-1 so every formula has a sensible level.
    double mean = 0.0;
    for (auto v : values) mean += v;
    mean /= tableSize;

    double peak = 0.0;
    for (auto& v : values)
    {
        v -= mean;
        peak = juce::jmax (peak, std::abs (v));
    }

    const double scale = peak > 1.0e-9 ? 1.0 / peak : 0.0;

    const int writeIndex = 1 - activeTable.load (std::memory_order_acquire);
    auto& dest = tables[(size_t) writeIndex];

    for (size_t i = 0; i < (size_t) tableSize; ++i)
        dest[i] = (float) (values[i] * scale);

    activeTable.store (writeIndex, std::memory_order_release);
    return {};
}
