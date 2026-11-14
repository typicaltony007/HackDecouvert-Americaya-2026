#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <atomic>

//==============================================================================
/**
    A single-cycle wavetable generated from a user-typed math expression.

    The expression is evaluated with x running from 0 to 2*pi across one cycle,
    e.g.  "sin(x) + 0.3*sin(5*x)"  or  "sign(sin(x)) * pow(abs(sin(x)), 0.3)".

    Supported: + - * /, parentheses, constants pi / e, the variable x and the
    functions sin cos tan abs min max pow sqrt exp log tanh sign floor frac
    saw(x) square(x) tri(x) noise().

    Threading: rebuild() runs on the message thread and writes to whichever of
    the two buffers is not currently being read, then atomically publishes it.
    The audio thread only ever calls getSample().
*/
class CustomWaveTable
{
public:
    static constexpr int tableSize = 2048;

    CustomWaveTable();

    /** Parses + renders the formula. Returns an empty string on success,
        otherwise an error message (and the previous table stays active). */
    juce::String rebuild (const juce::String& formula);

    /** Linear-interpolated lookup, phase in [0, 1). Audio-thread safe. */
    float getSample (double phase) const noexcept
    {
        const auto& table = tables[(size_t) activeTable.load (std::memory_order_acquire)];
        const double pos  = phase * tableSize;
        const int i0      = (int) pos;
        const float frac  = (float) (pos - i0);
        const int a       = i0 & (tableSize - 1);
        const int b       = (a + 1) & (tableSize - 1);
        return table[(size_t) a] + frac * (table[(size_t) b] - table[(size_t) a]);
    }

    /** Copies the current table (used by the UI to draw a preview). */
    void copyTable (std::array<float, tableSize>& dest) const noexcept
    {
        dest = tables[(size_t) activeTable.load (std::memory_order_acquire)];
    }

private:
    std::array<std::array<float, tableSize>, 2> tables {};
    std::atomic<int> activeTable { 0 };

    JUCE_DECLARE_NON_COPYABLE (CustomWaveTable)
};
