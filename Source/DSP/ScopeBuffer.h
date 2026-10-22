#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <vector>

namespace blg::dsp
{
/**
    A small ring buffer that lets a UI-thread visualiser see recent audio without the audio
    thread ever blocking or allocating.

    push() is real-time safe and must only be called from processBlock. readLatest() must only
    be called from the message thread. The two are not fully synchronised against each other
    (only the write position is atomic; the sample data itself is plain floats) -- for a
    cosmetic waveform/goniometer display, any rare tearing this could cause is visually
    imperceptible, which is the same tradeoff JUCE's own AudioVisualiserComponent makes, and it
    keeps push() allocation- and lock-free.
*/
class ScopeBuffer
{
public:
    void prepare (int numChannels, int capacityInSamples)
    {
        channels = juce::jlimit (1, maxChannels, numChannels);
        capacity = juce::jmax (64, capacityInSamples);

        for (auto& ch : data)
            ch.assign ((size_t) capacity, 0.0f);

        writePos.store (0, std::memory_order_relaxed);
    }

    void push (const juce::AudioBuffer<float>& buffer) noexcept
    {
        if (capacity == 0)
            return;

        const int numSamples      = buffer.getNumSamples();
        const int sourceChannels  = buffer.getNumChannels();
        int pos = writePos.load (std::memory_order_relaxed);

        for (int i = 0; i < numSamples; ++i)
        {
            for (int c = 0; c < channels; ++c)
            {
                const float* src = buffer.getReadPointer (juce::jmin (c, sourceChannels - 1));
                data[(size_t) c][(size_t) pos] = src[i];
            }

            if (++pos >= capacity)
                pos = 0;
        }

        writePos.store (pos, std::memory_order_relaxed);
    }

    // Fills outL/outR with the most recent `numSamples` samples (oldest first). Returns false
    // if prepare() hasn't been called yet. Safe to call only from the message thread.
    bool readLatest (std::vector<float>& outL, std::vector<float>& outR, int numSamples) const
    {
        if (capacity == 0)
            return false;

        numSamples = juce::jmin (numSamples, capacity);
        outL.resize ((size_t) numSamples);
        outR.resize ((size_t) numSamples);

        const int pos = writePos.load (std::memory_order_relaxed);
        int start = pos - numSamples;
        while (start < 0)
            start += capacity;

        const auto& L = data[0];
        const auto& R = data[(size_t) juce::jmin (1, channels - 1)];

        for (int i = 0; i < numSamples; ++i)
        {
            const int idx = (start + i) % capacity;
            outL[(size_t) i] = L[(size_t) idx];
            outR[(size_t) i] = R[(size_t) idx];
        }

        return true;
    }

private:
    static constexpr int maxChannels = 2;
    std::array<std::vector<float>, maxChannels> data;
    std::atomic<int> writePos { 0 };
    int channels = 2;
    int capacity = 0;
};
} // namespace blg::dsp
