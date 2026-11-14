#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <atomic>

//==============================================================================
/**
    Lock-free tap that lets the UI see the most recent output samples.

    The audio thread calls push() with the output buffer (summed to mono).
    The UI thread calls copyLatest() to read the newest N samples for its FFT.
    Each sample is stored as a relaxed atomic, so a read that overlaps a write
    can mix old and new samples. That's fine for a visual analyser.
*/
class SpectrumTap
{
public:
    static constexpr int capacity = 8192;   // power of two

    void push (const juce::AudioBuffer<float>& buffer) noexcept
    {
        const int numChannels = buffer.getNumChannels();
        const int numSamples  = buffer.getNumSamples();

        if (numChannels == 0)
            return;

        const float scale = 1.0f / (float) numChannels;
        int pos = writePos.load (std::memory_order_relaxed);

        for (int i = 0; i < numSamples; ++i)
        {
            float sum = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
                sum += buffer.getReadPointer (ch)[i];

            ring[(size_t) pos].store (sum * scale, std::memory_order_relaxed);
            pos = (pos + 1) & (capacity - 1);
        }

        writePos.store (pos, std::memory_order_release);
    }

    /** Copies the newest numSamples (<= capacity) into dest, oldest first. */
    void copyLatest (float* dest, int numSamples) const noexcept
    {
        jassert (numSamples <= capacity);
        int pos = (writePos.load (std::memory_order_acquire) - numSamples) & (capacity - 1);

        for (int i = 0; i < numSamples; ++i)
        {
            dest[i] = ring[(size_t) pos].load (std::memory_order_relaxed);
            pos = (pos + 1) & (capacity - 1);
        }
    }

private:
    std::array<std::atomic<float>, capacity> ring {};
    std::atomic<int> writePos { 0 };
};
