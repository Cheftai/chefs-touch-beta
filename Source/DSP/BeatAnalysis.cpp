#include "BeatAnalysis.h"
#include <juce_dsp/juce_dsp.h>
#include <numeric>

namespace kitchen
{
namespace
{
double estimateTempo(std::vector<float>& flux, double framesPerSecond)
{
    if (flux.size() < static_cast<size_t>(framesPerSecond*3.0))
        return 0;
    const auto mean = std::accumulate(flux.begin(), flux.end(), 0.0) / static_cast<double>(flux.size());
    if (mean < 0.003)
        return 0;
    for (auto& x : flux)
        x = std::max(0.0f, x - static_cast<float>(mean));
    const auto first = static_cast<int>(framesPerSecond*60/200);
    const auto last = static_cast<int>(framesPerSecond*60/60);
    std::vector<double> score(static_cast<size_t>(last+2));
    for (int lag = first-1; lag <= last+1; ++lag)
    {
        double cross = 0, a2 = 0, b2 = 0;
        for (size_t i = static_cast<size_t>(lag); i < flux.size(); ++i)
        {
            const auto a = flux[i];
            const auto b = flux[i-static_cast<size_t>(lag)];
            cross += a*b;
            a2 += a*a;
            b2 += b*b;
        }
        score[static_cast<size_t>(lag)] = cross/std::sqrt(1.0e-12+a2*b2);
    }
    int best = 0;
    double bestScore = 0.16;
    for (int lag = first; lag <= last; ++lag)
    {
        const auto x = score[static_cast<size_t>(lag)];
        if (x < score[static_cast<size_t>(lag-1)] || x < score[static_cast<size_t>(lag+1)])
            continue;
        const auto bpm = 60*framesPerSecond/lag;
        const auto weighted = x * (0.8 + 0.2*std::exp(-2*std::pow(std::log2(bpm/115), 2)));
        if (weighted > bestScore)
        {
            best = lag;
            bestScore = weighted;
        }
    }
    if (best == 0)
        return 0;
    const auto a = score[static_cast<size_t>(best-1)];
    const auto b = score[static_cast<size_t>(best)];
    const auto c = score[static_cast<size_t>(best+1)];
    const auto offset = a+c < 2*b-1e-9 ? 0.5*(a-c)/(a-2*b+c) : 0;
    return 60*framesPerSecond/(best+offset);
}

void estimateKey(BeatResult& result, const std::array<double, 12>& chroma, const std::array<double, 12>& bass)
{
    const auto total = std::accumulate(chroma.begin(), chroma.end(), 0.0);
    if (total < 1e-6)
        return;
    const auto bassTotal = std::accumulate(bass.begin(), bass.end(), 1.0e-8);
    const auto active = std::count_if(chroma.begin(), chroma.end(), [=](double value) { return value > 0.075*total; });
    if (active < 3)
        return;
    // Scale membership rewards the set of notes, while bass and tonic triad help separate relative keys.
    constexpr std::array<double, 12> major { 2.2,-1.5,0.25,-1.5,1.25,0.65,-1.5,1.45,-1.5,0.4,-1.5,0.30 };
    constexpr std::array<double, 12> minor { 2.2,-1.5,0.25,1.25,-1.5,0.65,-1.5,1.45,0.4,-1.5,0.3,-0.5 };
    double best = -10, second = -10;
    for (int root = 0; root < 12; ++root)
        for (bool isMinor : {false, true})
        {
            const auto& profile = isMinor ? minor : major;
            double score = 0.45*bass[static_cast<size_t>(root)]/bassTotal;
            for (size_t degree = 0; degree < profile.size(); ++degree)
                score += profile[degree]*chroma[(static_cast<size_t>(root)+degree)%12]/total;
            if (score > best)
            {
                second = best;
                best = score;
                result.key = root;
                result.minor = isMinor;
            }
            else
                second = std::max(second, score);
        }
    if (best < 0.65)
        result.key = -1;
    result.ambiguous = best-second < 0.10;
}

void accumulateChroma(const std::vector<float>& spectrum, double hzPerBin,
                      std::array<double, 12>& chroma, std::array<double, 12>& bass)
{
    const auto low = std::max(2, static_cast<int>(55/hzPerBin));
    const auto high = std::min(static_cast<int>(spectrum.size()/4)-2, static_cast<int>(1750/hzPerBin));
    float maximum = 0;
    for (int bin = low; bin <= high; ++bin)
        maximum = std::max(maximum, spectrum[static_cast<size_t>(bin)]);
    if (maximum < 0.08f)
        return;
    for (int bin = low; bin <= high; ++bin)
    {
        const auto k = static_cast<size_t>(bin);
        const auto a = spectrum[k-1], b = spectrum[k], c = spectrum[k+1];
        if (b < 0.055f*maximum || b <= a || b <= c)
            continue;
        const auto offset = 0.5*(a-c)/(a-2*b+c);
        const auto frequency = (bin+offset)*hzPerBin;
        if (frequency < 55)
            continue;
        const auto note = 69+12*std::log2(frequency/440.0);
        const auto rounded = std::round(note);
        const auto cls = static_cast<size_t>((static_cast<int>(rounded)+120)%12);
        const auto weight = (b/maximum)*(1-1.3*std::abs(note-rounded));
        chroma[cls] += weight;
        if (frequency < 220)
            bass[cls] += weight;
    }
}
}

BeatResult analyseBeat(const juce::File& file, const std::function<bool()>& cancelled)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    auto reader = std::unique_ptr<juce::AudioFormatReader>(formats.createReaderFor(file));
    BeatResult result;
    if (reader == nullptr || reader->sampleRate < 8000 || reader->sampleRate > 384000)
    {
        result.error = "Couldn't decode this beat";
        return result;
    }

    constexpr int order = 12, size = 1 << order, hop = 256;
    const int decimation = std::max(1, static_cast<int>(reader->sampleRate/12000));
    const auto rate = reader->sampleRate/decimation;
    const auto hopSize = hop*decimation;
    const auto frames = std::min<int64_t>(reader->lengthInSamples, static_cast<int64_t>(reader->sampleRate*60));
    const bool stereo = reader->numChannels > 1;
    juce::AudioBuffer<float> audio(stereo ? 2 : 1, hopSize*16);
    juce::dsp::FFT fft(order);
    juce::dsp::WindowingFunction<float> window(size, juce::dsp::WindowingFunction<float>::hann, false);
    std::vector<float> spectrum(2*size), flux;
    std::array<float, size> time {};
    std::array<float, 5> previous {};
    std::array<double, 12> chroma {}, bass {};
    int filled = 0, offset = 0;
    const auto cutoff = std::exp(-2*juce::MathConstants<double>::pi*0.40/decimation);
    float lowpass1 = 0, lowpass2 = 0;
    for (int64_t position = 0; position+hopSize <= frames && !cancelled();)
    {
        const auto length = static_cast<int>(std::min<int64_t>(audio.getNumSamples(), frames-position));
        if (!reader->read(&audio, 0, length, position, true, stereo))
        {
            result.error = "Couldn't read the beat";
            return result;
        }
        for (int i = 0; i+hopSize <= length; i += hopSize)
        {
            for (int s = i; s < i+hopSize; ++s)
            {
                const float left = audio.getSample(0, s);
                const float right = stereo ? audio.getSample(1, s) : left;
                const auto mid = (left+right)*0.5f;
                const auto clean = std::isfinite(mid) ? mid : 0.0f;
                const auto coeff = static_cast<float>(1-cutoff);
                lowpass1 += coeff*(clean-lowpass1);
                lowpass2 += coeff*(lowpass1-lowpass2);
                if ((s-i)%decimation == decimation-1)
                {
                    time[static_cast<size_t>(offset)] = lowpass2;
                    offset = (offset+1)%size;
                }
            }
            filled = std::min(size, filled+hop);
            if (filled < size)
                continue;
            std::copy(time.begin()+offset, time.end(), spectrum.begin());
            std::copy(time.begin(), time.begin()+offset, spectrum.begin()+size-offset);
            window.multiplyWithWindowingTable(spectrum.data(), size);
            fft.performFrequencyOnlyForwardTransform(spectrum.data(), true);
            constexpr std::array<double, 6> edges { 28, 145, 350, 1100, 3000, 5500 };
            float onset = 0;
            for (size_t band = 0; band < previous.size(); ++band)
            {
                const auto first = static_cast<int>(edges[band]*size/rate);
                const auto last = std::min(size/2, static_cast<int>(edges[band+1]*size/rate));
                float energy = 0;
                for (int bin = first; bin < last; ++bin)
                    energy += spectrum[static_cast<size_t>(bin)]*spectrum[static_cast<size_t>(bin)];
                const auto value = std::log1p(0.04f*std::sqrt(energy));
                onset += (band == 0 ? 1.8f : 1.0f)*std::max(0.0f, value-previous[band]);
                previous[band] = value;
            }
            flux.push_back(onset);
            if (flux.size()%3 == 0)
                accumulateChroma(spectrum, rate/size, chroma, bass);
        }
        position += length;
    }
    result.bpm = estimateTempo(flux, rate/hop);
    estimateKey(result, chroma, bass);
    return result;
}
}

