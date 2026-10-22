#pragma once
#include <JuceHeader.h>
#include <array>
#include <memory>
#include <vector>

namespace blg::dsp
{
/**
    Spectral feedback, the DSP behind the Feedback panel.

    The signal is cut into overlapping frames (1024-point FFT, 75% overlap, Hann windows). Each
    frame's spectrum X is mixed with the previous frame's *output* spectrum Y_prev:

        Y = X + g[k] * R(Y_prev)            (Y is stored for the next frame)

    where R() rotates every bin by the phase a steady tone at that bin would advance over one hop
    (so sustained, pitched content keeps reinforcing itself instead of cancelling) and optionally
    moves the whole spectrum sideways by `shiftBins` bins on every pass (a frequency-shifter in
    the loop, so repeats spiral up or down). g[k] = amount * tiltMultiplier[k], capped below 1 so
    the loop is always stable; `tilt` leans the feedback towards the low (-1) or high (+1) end.

    The output spectrum is overlap-added back into the time domain. A one-sided energy limiter
    stops the wet signal from growing past ~3x the input energy (tails may still ring out freely
    after the input stops), so high feedback settings cannot run away in level.

    With amount = 0 the stage is a transparent pure delay. Latency is fixed at fftSize - 1
    samples regardless of the settings, so hosts only ever need to compensate once.

    Real-time safe: all buffers are allocated in prepare(); process() never allocates or locks.
*/
class SpectralFeedback
{
public:
    static constexpr int fftOrder       = 10;
    static constexpr int fftSize        = 1 << fftOrder;      // 1024
    static constexpr int hopSize        = fftSize / 4;        // 75% overlap
    static constexpr int numBins        = fftSize / 2 + 1;
    static constexpr int latencySamples = fftSize - 1;
    static constexpr int maxShiftBins   = 8;

    struct Settings
    {
        float amount    = 0.0f;   // 0 .. 1
        float tilt      = 0.0f;   // -1 .. +1
        int   shiftBins = 0;      // -maxShiftBins .. +maxShiftBins
    };

    void prepare (double newSampleRate, int /*maxBlockSize*/)
    {
        sampleRate = newSampleRate;
        fft = std::make_unique<juce::dsp::FFT> (fftOrder);

        window.resize ((size_t) fftSize);
        for (int n = 0; n < fftSize; ++n)   // periodic Hann
            window[(size_t) n] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) n / (float) fftSize);

        calibrateFft();

        for (auto& c : chan)
            c.allocate();

        tiltMul.assign ((size_t) numBins, 1.0f);
        cachedTilt = 0.0f;
        amountSmoothed = 0.0f;
        reset();
    }

    void reset()
    {
        for (auto& c : chan)
            c.clear();

        amountSmoothed = 0.0f;
    }

    void process (juce::AudioBuffer<float>& buffer, const Settings& s) noexcept
    {
        const int numChannels = juce::jmin (buffer.getNumChannels(), maxChannels);
        const int numSamples  = buffer.getNumSamples();
        if (fft == nullptr || numChannels == 0 || numSamples == 0)
            return;

        // block-rate smoothing of the controls (~30 ms) so automation doesn't click
        const float coeff = juce::jmin (1.0f, (float) numSamples / (0.03f * (float) sampleRate));
        amountSmoothed += (juce::jlimit (0.0f, 1.0f, s.amount) - amountSmoothed) * coeff;

        const float tilt = juce::jlimit (-1.0f, 1.0f, s.tilt);
        if (std::abs (tilt - cachedTilt) > 1.0e-4f)
            rebuildTilt (tilt);

        const int   shift    = juce::jlimit (-maxShiftBins, maxShiftBins, s.shiftBins);
        const float baseGain = amountSmoothed * maxLoopGain;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* data = buffer.getWritePointer (ch);
            auto& c = chan[(size_t) ch];

            for (int i = 0; i < numSamples; ++i)
                data[i] = tick (c, data[i], baseGain, shift);
        }
    }

private:
    static constexpr int   maxChannels = 2;
    static constexpr int   mask        = fftSize - 1;
    static constexpr float maxLoopGain = 0.97f;
    static constexpr float maxEnergyRatio = 3.0f;

    struct Channel
    {
        std::vector<float> inRing, outRing, work, prevA, prevB;
        int   pos = 0, hopCount = 0;
        float limitScale = 1.0f, lastEnergy = 0.0f;

        void allocate()
        {
            inRing.assign  ((size_t) fftSize, 0.0f);
            outRing.assign ((size_t) fftSize, 0.0f);
            work.assign    ((size_t) fftSize * 2, 0.0f);
            prevA.assign   ((size_t) numBins * 2, 0.0f);
            prevB.assign   ((size_t) numBins * 2, 0.0f);
        }

        void clear()
        {
            for (auto* v : { &inRing, &outRing, &work, &prevA, &prevB })
                std::fill (v->begin(), v->end(), 0.0f);

            pos = hopCount = 0;
            limitScale = 1.0f;
            lastEnergy = 0.0f;
        }
    };

    // JUCE's FFT backends all promise forward -> inverse round-trips to the original, but the
    // exact scaling differs by platform, so measure it once instead of assuming.
    void calibrateFft()
    {
        std::vector<float> original ((size_t) fftSize), test ((size_t) fftSize * 2, 0.0f);
        for (int n = 0; n < fftSize; ++n)
        {
            original[(size_t) n] = std::sin (0.05f * (float) n) + 0.5f * std::sin (0.31f * (float) n + 1.0f);
            test[(size_t) n]     = original[(size_t) n];
        }

        fft->performRealOnlyForwardTransform (test.data(), true);
        fft->performRealOnlyInverseTransform (test.data());

        double dot = 0.0, ref = 0.0;
        for (int n = 0; n < fftSize; ++n)
        {
            dot += (double) test[(size_t) n] * (double) original[(size_t) n];
            ref += (double) original[(size_t) n] * (double) original[(size_t) n];
        }

        const float roundTripScale = (dot > 1.0e-9 && ref > 1.0e-9) ? (float) (ref / dot) : 1.0f;
        outputNorm = roundTripScale / 1.5f;   // 1.5 = sum of squared Hann windows at 75% overlap
    }

    void rebuildTilt (float tilt)
    {
        cachedTilt = tilt;
        const float norm = std::log2 (1.0f + (float) (numBins - 1));

        for (int k = 0; k < numBins; ++k)
        {
            const float p  = std::log2 (1.0f + (float) k) / norm;       // 0 .. 1 on a log-frequency axis
            const float dB = tilt * 12.0f * (2.0f * p - 1.0f);          // +/-12 dB at the extremes
            tiltMul[(size_t) k] = std::pow (10.0f, dB / 20.0f);
        }
    }

    float tick (Channel& c, float x, float baseGain, int shift) noexcept
    {
        c.inRing[(size_t) c.pos] = x;

        if (++c.hopCount >= hopSize)
        {
            c.hopCount = 0;
            processFrame (c, baseGain, shift);
        }

        // the oldest slot of the window just completed is final: nothing will add to it again
        const int outSlot = (c.pos + 1) & mask;
        const float y = c.outRing[(size_t) outSlot];
        c.outRing[(size_t) outSlot] = 0.0f;

        c.pos = (c.pos + 1) & mask;
        return y;
    }

    void processFrame (Channel& c, float baseGain, int shift) noexcept
    {
        float* d = c.work.data();
        const int start = (c.pos + 1) & mask;

        for (int n = 0; n < fftSize; ++n)
            d[n] = c.inRing[(size_t) ((start + n) & mask)] * window[(size_t) n];

        fft->performRealOnlyForwardTransform (d, true);

        const float* prevOld = c.prevA.data();
        float*       prevNew = c.prevB.data();
        double energyIn = 0.0, energyOut = 0.0;

        for (int k = 0; k < numBins; ++k)
        {
            const float xr = d[2 * k], xi = d[2 * k + 1];

            float fr = 0.0f, fi = 0.0f;
            const int src = k - shift;
            if (src >= 0 && src < numBins)
            {
                const float pr = prevOld[2 * src], pi = prevOld[2 * src + 1];
                switch (k & 3)   // phase advance of bin k over one hop (hop = fftSize/4): j^k
                {
                    case 0:  fr =  pr; fi =  pi; break;
                    case 1:  fr = -pi; fi =  pr; break;
                    case 2:  fr = -pr; fi = -pi; break;
                    default: fr =  pi; fi = -pr; break;
                }
            }

            const float g  = juce::jmin (maxLoopGain, baseGain * tiltMul[(size_t) k]);
            const float yr = xr + g * fr, yi = xi + g * fi;

            prevNew[2 * k] = yr;  prevNew[2 * k + 1] = yi;
            d[2 * k]       = yr;  d[2 * k + 1]       = yi;

            energyIn  += (double) xr * xr + (double) xi * xi;
            energyOut += (double) yr * yr + (double) yi * yi;
        }

        // DC and Nyquist bins of a real signal are purely real
        d[1] = 0.0f;  d[2 * (numBins - 1) + 1] = 0.0f;
        prevNew[1] = 0.0f;  prevNew[2 * (numBins - 1) + 1] = 0.0f;

        // one-sided limiter: wet energy may not grow past ~3x the input's (or its own previous
        // level, which lets tails decay naturally after the input stops)
        const float allowed = juce::jmax (maxEnergyRatio * (float) energyIn, 0.995f * c.lastEnergy);
        float target = 1.0f;
        if ((float) energyOut > allowed && energyOut > 1.0e-12)
            target = std::sqrt (allowed / (float) energyOut);

        c.limitScale = target < c.limitScale ? target : c.limitScale + (target - c.limitScale) * 0.3f;
        c.lastEnergy = (float) energyOut * c.limitScale * c.limitScale;

        fft->performRealOnlyInverseTransform (d);

        const float norm = c.limitScale * outputNorm;
        for (int n = 0; n < fftSize; ++n)
            c.outRing[(size_t) ((start + n) & mask)] += d[n] * window[(size_t) n] * norm;

        c.prevA.swap (c.prevB);
    }

    double sampleRate = 44100.0;
    float  outputNorm = 1.0f / 1.5f;
    float  cachedTilt = 0.0f;
    float  amountSmoothed = 0.0f;

    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> window, tiltMul;
    std::array<Channel, maxChannels> chan;
};
} // namespace blg::dsp
