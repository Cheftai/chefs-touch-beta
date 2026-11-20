#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>

namespace kitchen
{
struct Controls
{
    int mode, key, scale, quality;
    float input, output, retune, human, low, mid, high, threshold, ratio,
          deess, room, echo, time, drive, mix, width, ceiling, tune, denoise, noiseFloor;
    bool hardLock;
    int manualNote = -1;
};

struct Meter
{
    float input = 0, left = 0, right = 0, pitch = 0, reduction = 0;
    float pitchConfidence = 0, targetCents = 0;
};

class Processing
{
public:
    void prepare(double sampleRate, int channels);
    void reset();
    Meter process(juce::AudioBuffer<float>&, const Controls&, bool bypass);
    int latency() const { return delay; }

private:
    struct Filter
    {
        void configure(int type, double rate, float hz, float db);
        float tick(float x);
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    };

    void trackPitch(float mono, const Controls&);
    float correction(const Controls&);
    float interpolate(const std::vector<float>&, int position, int lag, float fraction, bool cubic) const;
    float shift(int channel, float span, float amount, bool cubic) const;
    void resetEffects();
    float estimatePeak(int channel, float sample, int quality);

    std::array<std::vector<float>, 2> history, echoLine, masterLine;
    std::array<std::array<Filter, 3>, 2> eq;
    static constexpr size_t peakTaps = 16;
    std::array<std::array<float, peakTaps>, 2> peakHistory {};
    std::array<std::array<float, peakTaps>, 7> peakKernels {};
    std::array<float, 2> sibilance {};
    std::array<float, 2> rumble {}, lowNoise {}, highNoise {};
    std::array<float, 3> cleanLevels {}, cleanGains { 1, 1, 1 };
    juce::Reverb reverb;
    juce::SmoothedValue<float> inputGain, outputGain, wetMix, echoTime, cleanup;
    std::vector<float> detector;
    struct Peak { uint64_t sample; float value; };
    std::vector<Peak> peaks;
    size_t peakFront = 0, peakEnd = 0;
    uint64_t sampleIndex = 0;
    double rate = 48000;
    int delay = 0, write = 0, echoWrite = 0, channels = 2, lastMode = -1;
    int decimation = 4, decimateCount = 0, detectWrite = 0, detectFilled = 0, hop = 0;
    int lockedNote = -1, lockedKey = -1, lockedScale = -1;
    float detectionRate = 12000, detectorCoefficient = 0, detectorLP = 0, frequency = 0, confidence = 0;
    float midPower = 0, sidePower = 0;
    float phase = 0, pitchRatio = 1, envelope = 0, sEnvelope = 0, limitGain = 1;
    float grainSpan = 0;
    float targetCents = 0;
};
}

