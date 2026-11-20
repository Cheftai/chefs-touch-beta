#include "Processing.h"

namespace kitchen
{
namespace
{
float gain(float db) { return juce::Decibels::decibelsToGain(db); }
float db(float amplitude) { return juce::Decibels::gainToDecibels(amplitude); }
float cubicAt(float a, float b, float c, float d, float t)
{
    return b + 0.5f * t * (c - a + t * (2*a - 5*b + 4*c - d + t * (3*(b-c) + d-a)));
}
float smoothCoefficient(double rate, float ms)
{
    return std::exp(-1000.0f / (static_cast<float>(rate) * ms));
}
}

void Processing::Filter::configure(int type, double rate, float hz, float level)
{
    using Coefficients = juce::dsp::IIR::ArrayCoefficients<float>;
    hz = std::min(hz, static_cast<float>(0.42 * rate));
    const auto a = type == 0 ? Coefficients::makeLowShelf(rate, hz, 0.707f, gain(level))
                 : type == 1 ? Coefficients::makePeakFilter(rate, hz, 0.8f, gain(level))
                             : Coefficients::makeHighShelf(rate, hz, 0.707f, gain(level));
    b0 = a[0]/a[3]; b1 = a[1]/a[3]; b2 = a[2]/a[3];
    a1 = a[4]/a[3]; a2 = a[5]/a[3];
}

float Processing::Filter::tick(float x)
{
    const auto y = b0*x + z1;
    z1 = b1*x - a1*y + z2;
    z2 = b2*x - a2*y;
    return y;
}

void Processing::prepare(double sampleRate, int count)
{
    rate = sampleRate;
    channels = count;
    delay = static_cast<int>(std::ceil(rate * 0.025));
    peaks.resize(static_cast<size_t>(delay) + peakTaps + 2);
    decimation = std::max(1, static_cast<int>(rate / 12000));
    detectionRate = static_cast<float>(rate) / static_cast<float>(decimation);
    detectorCoefficient = 1.0f - smoothCoefficient(rate, 0.12f);
    detector.resize(2*static_cast<size_t>(std::ceil(detectionRate * 0.075)));
    for (size_t phaseIndex = 0; phaseIndex < peakKernels.size(); ++phaseIndex)
    {
        auto& kernel = peakKernels[phaseIndex];
        const auto fraction = static_cast<double>(phaseIndex+1) / 8.0;
        float sum = 0;
        for (size_t tap = 0; tap < kernel.size(); ++tap)
        {
            const auto distance = static_cast<double>(tap) - 7.0 - fraction;
            const auto angle = juce::MathConstants<double>::pi * distance;
            const auto window = 0.42 + 0.50*std::cos(angle/8) + 0.08*std::cos(angle/4);
            kernel[tap] = static_cast<float>(window * std::sin(angle)/angle);
            sum += kernel[tap];
        }
        for (auto& tap : kernel)
            tap /= sum;
    }
    for (int ch = 0; ch < channels; ++ch)
    {
        history[ch].resize(static_cast<size_t>(2*delay + 8));
        masterLine[ch].resize(history[ch].size());
        echoLine[ch].resize(static_cast<size_t>(std::ceil(rate * 6.02)) + 8);
    }
    reverb.setSampleRate(rate);
    for (auto* smoother : { &inputGain, &outputGain, &wetMix, &echoTime, &cleanup })
        smoother->reset(rate, 0.025);
    reset();
}

void Processing::resetEffects()
{
    for (auto& line : echoLine)
        std::fill(line.begin(), line.end(), 0.0f);
    for (auto& line : masterLine)
        std::fill(line.begin(), line.end(), 0.0f);
    for (auto& channel : eq)
        for (auto& filter : channel)
            filter.z1 = filter.z2 = 0;
    reverb.reset();
    peakHistory = {};
    sibilance = {};
    rumble = lowNoise = highNoise = {};
    cleanLevels = {};
    cleanGains = { 1, 1, 1 };
    echoWrite = 0;
    peakFront = peakEnd = 0;
    sampleIndex = 0;
    limitGain = 1;
    envelope = sEnvelope = 0;
}

void Processing::reset()
{
    resetEffects();
    for (auto& line : history)
        std::fill(line.begin(), line.end(), 0.0f);
    write = decimateCount = detectWrite = detectFilled = hop = 0;
    detectorLP = frequency = confidence = phase = midPower = sidePower = 0;
    pitchRatio = 1;
    grainSpan = 0;
    lockedNote = -1;
    lastMode = -1;
}

void Processing::trackPitch(float mono, const Controls& p)
{
    detectorLP += (mono - detectorLP) * detectorCoefficient;
    if (++decimateCount < decimation)
        return;
    decimateCount = 0;
    const auto size = static_cast<int>(detector.size()/2);
    detector[static_cast<size_t>(detectWrite)] = detectorLP;
    detector[static_cast<size_t>(detectWrite+size)] = detectorLP;
    detectWrite = (detectWrite + 1) % size;
    detectFilled = std::min(size, detectFilled + 1);
    const int window = static_cast<int>(detectionRate * 0.028f);
    const int first = std::max(2, static_cast<int>(detectionRate / 950.0f));
    const int last = static_cast<int>(detectionRate / 65.0f);
    if (++hop < static_cast<int>(detectionRate * (p.quality ? 0.009f : 0.016f))
        || detectFilled < window + last + 1)
        return;
    hop = 0;
    const int stride = p.quality ? 1 : 2;
    const auto* newest = detector.data() + (detectWrite == 0 ? 2*size-1 : detectWrite+size-1);
    if (p.denoise > 0)
    {
        float power = 0;
        for (int i = 0; i < window; i += stride)
            power += newest[-i]*newest[-i];
        const auto floor = gain(p.noiseFloor);
        if (power*static_cast<float>(stride) < 0.5f*floor*floor*static_cast<float>(window))
        {
            confidence = 0;
            return;
        }
    }

    // Prefer the first strong local maximum to reject octave multiples.
    const auto score = [&](int lag)
    {
        float cross = 0, energy = 0;
        for (int i = 0; i < window; i += stride)
        {
            const auto a = newest[-i];
            const auto b = newest[-i-lag];
            cross += 2*a*b;
            energy += a*a + b*b;
        }
        return energy > 0.00002f ? cross/energy : 0.0f;
    };

    confidence = 0;
    float best = 0.70f;
    float previous = score(first - 1), current = score(first);
    for (int lag = first; lag <= last; ++lag)
    {
        const float next = score(lag + 1);
        if (current > previous && current >= next && current > best)
        {
            const auto curvature = previous - 2*current + next;
            const auto offset = std::abs(curvature) > 1.0e-6f
                              ? juce::jlimit(-0.5f, 0.5f, 0.5f*(previous-next)/curvature) : 0.0f;
            frequency = detectionRate / (static_cast<float>(lag) + offset);
            confidence = current;
            best = current;
            if (current > 0.91f)
                break;
        }
        previous = current;
        current = next;
    }
}

float Processing::correction(const Controls& p)
{
    if (confidence == 0 || frequency <= 0)
    {
        lockedNote = -1;
        return 1;
    }
    if (lockedKey != p.key || lockedScale != p.scale)
        lockedNote = -1;
    lockedKey = p.key;
    lockedScale = p.scale;
    constexpr int major = 0xAB5;
    constexpr int minor = 0x5AD;
    const int mask = p.scale == 1 ? major : p.scale == 2 ? minor : 0xFFF;
    const float note = 69.0f + 12.0f * std::log2(frequency / 440.0f);
    const int nearest = static_cast<int>(std::round(note));
    float distance = 12;
    int target = nearest;
    for (int candidate = nearest - 3; candidate <= nearest + 3; ++candidate)
    {
        const int degree = (candidate - p.key + 120) % 12;
        const float delta = static_cast<float>(candidate) - note;
        if ((mask & (1 << degree)) && std::abs(delta) < std::abs(distance))
        {
            distance = delta;
            target = candidate;
        }
    }
    const auto previousDistance = static_cast<float>(lockedNote)-note;
    const auto hysteresis = p.hardLock ? 0.04f+0.12f*p.human : 0.08f+0.24f*p.human;
    if (p.manualNote >= 0)
    {
        distance=juce::jlimit(-12.0f,12.0f,static_cast<float>(p.manualNote)-note);
        lockedNote=-1;
    }
    else if (lockedNote >= 0 && std::abs(previousDistance) <= std::abs(distance)+hysteresis)
        distance = previousDistance;
    else
        lockedNote = target;
    const auto preserve = (p.hardLock ? 0.45f : 0.85f)*p.human;
    targetCents = -100*distance;
    return std::exp2(distance * p.tune * (1.0f - preserve) / 12.0f);
}

float Processing::interpolate(const std::vector<float>& line, int position, int lag, float fraction, bool cubic) const
{
    const int size = static_cast<int>(line.size());
    const int index = position - lag + size;
    const auto b = line[static_cast<size_t>(index % size)];
    const auto c = line[static_cast<size_t>((index - 1) % size)];
    if (!cubic)
        return b + fraction * (c-b);
    return cubicAt(line[static_cast<size_t>((index + 1) % size)], b, c,
                   line[static_cast<size_t>((index - 2) % size)], fraction);
}

float Processing::shift(int channel, float span, float amount, bool cubic) const
{
    const auto& line = history[channel];
    const auto dry = interpolate(line, write, delay, 0, false);
    if (amount < 0.001f)
        return dry;
    const float other = phase < 0.5f ? phase + 0.5f : phase - 0.5f;
    const auto tap = [&](float position)
    {
        const auto lag = static_cast<float>(delay) + span*(position - 0.5f);
        const auto whole = static_cast<int>(lag);
        return interpolate(line, write, whole, lag - static_cast<float>(whole), cubic);
    };
    const auto weight = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * phase);
    const auto wet = weight*tap(phase) + (1-weight)*tap(other);
    return dry + amount*(wet-dry);
}

float Processing::estimatePeak(int channel, float x, int quality)
{
    auto& h = peakHistory[channel];
    std::move(h.begin()+1, h.end(), h.begin());
    h.back() = x;
    float peak = std::abs(x);
    // Standard uses the even phases of the same band-limited interpolator.
    const size_t stride = quality ? 1 : 2;
    for (size_t i = stride-1; i < peakKernels.size(); i += stride)
    {
        float interpolated = 0;
        for (size_t tap = 0; tap < h.size(); ++tap)
            interpolated += h[tap]*peakKernels[i][tap];
        peak = std::max(peak, std::abs(interpolated));
    }
    return peak;
}

Meter Processing::process(juce::AudioBuffer<float>& buffer, const Controls& p, bool bypass)
{
    juce::ScopedNoDenormals noDenormals;
    Meter meter;
    if (history[0].empty())
        return meter;
    const int mode = bypass ? 2 : p.mode;
    if (mode != lastMode)
    {
        resetEffects();
        detectFilled = hop = 0;
        frequency = confidence = phase = detectorLP = midPower = sidePower = 0;
        pitchRatio = 1;
        grainSpan = 0;
        lockedNote = -1;
    }
    if (lastMode < 0)
    {
        inputGain.setCurrentAndTargetValue(gain(p.input));
        outputGain.setCurrentAndTargetValue(gain(p.output));
        wetMix.setCurrentAndTargetValue(p.mix);
        echoTime.setCurrentAndTargetValue(p.time);
        cleanup.setCurrentAndTargetValue(p.denoise);
    }
    lastMode = mode;
    inputGain.setTargetValue(gain(p.input));
    outputGain.setTargetValue(gain(p.output));
    wetMix.setTargetValue(p.mix);
    echoTime.setTargetValue(p.time);
    cleanup.setTargetValue(p.denoise);

    for (auto& channel : eq)
    {
        channel[0].configure(0, rate, 180, p.low);
        channel[1].configure(1, rate, mode == 0 ? 2400.0f : 1100.0f, p.mid);
        channel[2].configure(2, rate, 6500, p.high);
    }
    juce::Reverb::Parameters room;
    room.roomSize = 0.22f + 0.64f*p.room;
    room.damping = 0.54f;
    room.wetLevel = 0.55f*p.room;
    room.dryLevel = 1.0f/2.0f; // JUCE scales its dry control by two.
    reverb.setParameters(room);
    const int historySize = static_cast<int>(history[0].size());
    const int echoSize = static_cast<int>(echoLine[0].size());
    const auto attack = smoothCoefficient(rate, 8), release = smoothCoefficient(rate, 100);
    const auto sAttack = smoothCoefficient(rate, 1), sRelease = smoothCoefficient(rate, 42);
    const auto limitRelease = smoothCoefficient(rate, 85);
    const auto retune = smoothCoefficient(rate, p.hardLock ? p.retune*0.18f : p.retune);
    const auto stereoAverage = 1.0f - smoothCoefficient(rate, 24);
    const auto split = 1.0f - smoothCoefficient(rate, 1000.0f / (2.0f * juce::MathConstants<float>::pi * 5750.0f));
    const auto ceiling = gain(p.ceiling) * 0.985f;
    const auto pole = [&](float hz) { return 1 - std::exp(-juce::MathConstants<float>::twoPi * hz / static_cast<float>(rate)); };
    const auto rumblePole = pole(62), lowPole = pole(330), highPole = pole(3500);
    const auto open = smoothCoefficient(rate, 0.75f), close = smoothCoefficient(rate, 75);
    const auto noiseFloor = gain(p.noiseFloor);
    const auto spanSmooth = smoothCoefficient(rate, 2.5f);
    const float drive = 1 + 5*p.drive;
    const float driveNorm = 1/std::tanh(drive);
    float targetRatio = correction(p);
    float span = 2 * static_cast<float>(rate) / std::max(65.0f, frequency);
    int previousHop = hop;
    float* samples[2] { buffer.getWritePointer(0), channels == 2 ? buffer.getWritePointer(1) : nullptr };
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        float dry[2] {}, wet[2] {};
        const auto in = inputGain.getNextValue();
        const auto out = outputGain.getNextValue();
        const auto mix = wetMix.getNextValue();
        const auto echoLag = static_cast<float>(rate) * echoTime.getNextValue() * 0.001f;
        float mono = 0;
        for (int ch = 0; ch < channels; ++ch)
        {
            const auto x = samples[ch][i];
            meter.input = std::max(meter.input, std::abs(x));
            history[ch][static_cast<size_t>(write)] = x;
            dry[ch] = interpolate(history[ch], write, delay, 0, false);
            mono += x;
        }

        if (mode == 0)
        {
            if (channels == 2)
            {
                const auto side = samples[0][i] - samples[1][i];
                midPower += stereoAverage * (mono*mono - midPower);
                sidePower += stereoAverage * (side*side - sidePower);
                if (midPower < 0.04f*sidePower)
                    mono = 2*samples[0][i];
            }
            trackPitch(mono * in / static_cast<float>(channels), p);
            if (hop < previousHop)
            {
                targetRatio = correction(p);
                span = 2 * static_cast<float>(rate) / std::max(65.0f, frequency);
            }
            previousHop = hop;
            pitchRatio = targetRatio + retune*(pitchRatio - targetRatio);
            grainSpan = grainSpan == 0 ? span : span + spanSmooth*(grainSpan-span);
            phase += (1-pitchRatio) / grainSpan;
            phase -= std::floor(phase);
        }

        if (mode != 2)
        {
            float peak = 0, sPeak = 0, highBand[2] {};
            const auto clean = cleanup.getNextValue();
            if (mode == 0)
            {
                float bands[2][3] {};
                std::array<float, 3> bandPeaks {};
                for (int ch = 0; ch < channels; ++ch)
                {
                    const auto x = in * shift(ch, grainSpan, std::min(1.0f, 250.0f*std::abs(pitchRatio-1)), p.quality != 0);
                    wet[ch] = x;
                    if (clean <= 0)
                        continue;
                    rumble[ch] += rumblePole*(x-rumble[ch]);
                    const auto filtered = x-rumble[ch];
                    lowNoise[ch] += lowPole*(filtered-lowNoise[ch]);
                    highNoise[ch] += highPole*(filtered-highNoise[ch]);
                    bands[ch][0] = lowNoise[ch];
                    bands[ch][1] = highNoise[ch]-lowNoise[ch];
                    bands[ch][2] = filtered-highNoise[ch];
                    for (size_t b = 0; b < bandPeaks.size(); ++b)
                        bandPeaks[b] = std::max(bandPeaks[b], std::abs(bands[ch][b]));
                }
                if (clean > 0)
                {
                    for (size_t b = 0; b < bandPeaks.size(); ++b)
                    {
                        const auto peakBand = bandPeaks[b];
                        auto& level = cleanLevels[b];
                        level = peakBand + (peakBand > level ? open : close)*(level-peakBand);
                    }
                    for (size_t b = 0; b < bandPeaks.size(); ++b)
                    {
                        const auto activity = std::max(cleanLevels[b], 0.16f*cleanLevels[1]);
                        const auto gate = juce::jlimit(0.0f, 1.0f, (activity/noiseFloor-0.75f)*0.8f);
                        const auto target = 0.035f + 0.965f*gate*gate*(3-2*gate);
                        auto& bandGain = cleanGains[b];
                        bandGain = target + (target > bandGain ? open : close)*(bandGain-target);
                    }
                    for (int ch = 0; ch < channels; ++ch)
                    {
                        const auto filtered = bands[ch][0]*cleanGains[0]+bands[ch][1]*cleanGains[1]+bands[ch][2]*cleanGains[2];
                        wet[ch] += clean*(filtered-wet[ch]);
                    }
                }
            }
            for (int ch = 0; ch < channels; ++ch)
            {
                auto x = mode == 0 ? wet[ch] : in * samples[ch][i];
                for (auto& band : eq[ch])
                    x = band.tick(x);
                wet[ch] = x;
                peak = std::max(peak, std::abs(x));
                sibilance[ch] += split * (x - sibilance[ch]);
                highBand[ch] = x - sibilance[ch];
                sPeak = std::max(sPeak, std::abs(highBand[ch]));
            }
            envelope = peak + (peak > envelope ? attack : release)*(envelope-peak);
            sEnvelope = sPeak + (sPeak > sEnvelope ? sAttack : sRelease)*(sEnvelope-sPeak);
            const auto reduction = std::max(0.0f, db(envelope) - p.threshold) * (1 - 1/p.ratio);
            const auto compress = gain(-reduction);
            const auto deess = mode == 0 ? 1-gain(-p.deess * juce::jlimit(0.0f, 12.0f, db(sEnvelope) + 30.0f)) : 0;
            meter.reduction = std::max(meter.reduction, reduction);

            for (int ch = 0; ch < channels; ++ch)
            {
                const auto x = compress * (wet[ch] - deess*highBand[ch]);
                wet[ch] = p.drive > 0 ? x + p.drive * (std::tanh(x*drive)*driveNorm - x) : x;
                if (mode == 0)
                {
                    const auto whole = static_cast<int>(echoLag);
                    const auto repeat = interpolate(echoLine[ch], echoWrite, whole, echoLag-static_cast<float>(whole), false);
                    echoLine[ch][static_cast<size_t>(echoWrite)] = wet[ch] + 0.42f*p.echo*repeat;
                    wet[ch] += p.echo*repeat;
                }
            }

            if (mode == 0)
            {
                if (channels == 2)
                    reverb.processStereo(&wet[0], &wet[1], 1);
                else
                    reverb.processMono(&wet[0], 1);
                echoWrite = (echoWrite+1) % echoSize;
            }
            else
            {
                if (channels == 2)
                {
                    const auto mid = (wet[0]+wet[1])*0.5f;
                    const auto side = (wet[0]-wet[1])*0.5f*p.width;
                    wet[0] = mid+side;
                    wet[1] = mid-side;
                }
                float reconstructedPeak = 0;
                for (int ch = 0; ch < channels; ++ch)
                    reconstructedPeak = std::max(reconstructedPeak, estimatePeak(ch, wet[ch]*out, p.quality));
                // Keep the largest predicted peak across the entire lookahead window.
                while (peakFront != peakEnd
                       && peaks[peakFront].sample + static_cast<uint64_t>(delay) + peakTaps < sampleIndex)
                    peakFront = (peakFront+1) % peaks.size();
                while (peakFront != peakEnd)
                {
                    const auto previous = (peakEnd + peaks.size()-1) % peaks.size();
                    if (peaks[previous].value > reconstructedPeak)
                        break;
                    peakEnd = previous;
                }
                peaks[peakEnd] = { sampleIndex++, reconstructedPeak };
                peakEnd = (peakEnd+1) % peaks.size();
                const float windowPeak = peaks[peakFront].value;
                const float required = windowPeak > ceiling ? ceiling/windowPeak : 1;
                limitGain = std::min(required, 1 + limitRelease*(limitGain-1));
                meter.reduction = std::max(meter.reduction, -db(limitGain));

                for (int ch = 0; ch < channels; ++ch)
                {
                    masterLine[ch][static_cast<size_t>(write)] = wet[ch]*out;
                    wet[ch] = juce::jlimit(-ceiling, ceiling,
                        limitGain * interpolate(masterLine[ch], write, delay, 0, false));
                }
            }
        }

        for (int ch = 0; ch < channels; ++ch)
        {
            const auto y = mode == 2 ? dry[ch] : mode == 1 ? wet[ch]
                                      : out*(dry[ch] + mix*(wet[ch]-dry[ch]));
            samples[ch][i] = y;
            auto& value = ch == 0 ? meter.left : meter.right;
            value = std::max(value, std::abs(y));
        }
        write = (write+1) % historySize;
    }
    if (channels == 1)
        meter.right = meter.left;
    meter.pitch = confidence > 0 ? frequency : 0;
    meter.pitchConfidence = confidence;
    meter.targetCents = confidence > 0 ? targetCents : 0;
    return meter;
}
}

