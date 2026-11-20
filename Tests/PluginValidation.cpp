#include <juce_audio_processors/juce_audio_processors.h>
#include "../Source/DSP/BeatAnalysis.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace
{
constexpr auto rate = 48000.0;
constexpr int blockSize = 257;

void require(bool ok, const juce::String& message)
{
    if (!ok)
        throw std::runtime_error(message.toStdString());
}

juce::AudioProcessorParameter& parameter(juce::AudioPluginInstance& plugin, const char* name)
{
    for (auto* item : plugin.getParameters())
        if (item->getName(128) == name)
            return *item;
    throw std::runtime_error((juce::String("Missing automation parameter ") + name).toStdString());
}

void set(juce::AudioPluginInstance& plugin, const char* name, float normalized)
{
    parameter(plugin, name).setValueNotifyingHost(normalized);
}

void process(juce::AudioPluginInstance& plugin, double seconds,
             const std::function<float(int, int)>& input, const std::function<void(int, int, float)>& output)
{
    const auto length = static_cast<int>(seconds*rate);
    const auto channels = plugin.getTotalNumInputChannels();
    juce::AudioBuffer<float> audio(channels, blockSize);
    juce::MidiBuffer midi;
    for (int position = 0; position < length; position += blockSize)
    {
        const auto size = std::min(blockSize, length-position);
        audio.setSize(channels, size, false, false, true);
        for (int ch = 0; ch < channels; ++ch)
            for (int i = 0; i < size; ++i)
                audio.setSample(ch, i, input(ch, position+i));
        plugin.processBlock(audio, midi);
        for (int i = 0; i < size; ++i)
            for (int ch = 0; ch < channels; ++ch)
            {
                const float sample = audio.getSample(ch, i);
                if (!std::isfinite(sample))
                    throw std::runtime_error("Non-finite output");
                output(ch, position+i, sample);
            }
    }
}

float tone(float frequency, int position)
{
    return static_cast<float>(std::sin(juce::MathConstants<double>::twoPi * frequency * position / rate));
}

float rms(juce::AudioPluginInstance& plugin, float frequency, float amplitude)
{
    double energy = 0;
    int count = 0;
    process(plugin, 0.36, [=](int, int i) { return amplitude*tone(frequency, i); },
            [&](int ch, int i, float x)
            {
                if (ch == 0 && i >= static_cast<int>(rate*0.20))
                {
                    energy += x*x;
                    ++count;
                }
            });
    return static_cast<float>(std::sqrt(energy/count));
}

double correctedPitch(juce::AudioPluginInstance& plugin, float frequency)
{
    int crossings = 0;
    double first = 0, last = 0;
    float previous = 0;
    process(plugin, 1.3, [=](int, int i) { return 0.3f*tone(frequency, i); },
            [&](int ch, int i, float sample)
            {
                if (ch != 0 || i < rate/2)
                    return;
                if (previous < 0 && sample >= 0)
                {
                    last = i-1 - previous/(sample-previous);
                    if (++crossings == 1)
                        first = last;
                }
                previous = sample;
            });
    require(crossings > 2, "No stable output pitch");
    return (crossings-1)*rate / (last-first);
}

void checkVocalEffects(juce::AudioPluginInstance& plugin, int latency)
{
    const auto gainReference = rms(plugin, 440, 0.2f);
    set(plugin, "Input", 0.625f);
    require(rms(plugin, 440, 0.2f) > 1.95f*gainReference, "Vocal input trim failed");
    set(plugin, "Wet / Dry", 0);
    require(std::abs(rms(plugin, 440, 0.2f)-gainReference) < 0.005f, "Dry mix failed");
    set(plugin, "Output", 0.625f);
    require(rms(plugin, 440, 0.2f) > 1.95f*gainReference, "Output trim failed");
    set(plugin, "Input", 0.5f);
    set(plugin, "Output", 0.5f);
    set(plugin, "Wet / Dry", 1);

    const auto eqReference = rms(plugin, 110, 0.2f);
    set(plugin, "Low Shelf", 1);
    require(rms(plugin, 110, 0.2f) > 2.0f*eqReference, "Low shelf boost failed");
    set(plugin, "Low Shelf", 0.5f);
    set(plugin, "Comp Ratio", 1);
    set(plugin, "Comp Threshold", 0.30f);
    require(rms(plugin, 440, 0.30f) < 0.75f*gainReference, "Vocal compressor failed");
    set(plugin, "Comp Ratio", 0);

    const auto sReference = rms(plugin, 9600, 0.3f);
    set(plugin, "De-esser", 1);
    require(rms(plugin, 9600, 0.3f) < 0.7f*sReference, "De-esser failed");
    set(plugin, "De-esser", 0);
    set(plugin, "Saturation", 0.85f);
    require(rms(plugin, 440, 0.2f) > 1.5f*gainReference, "Saturation failed");
    set(plugin, "Saturation", 0);

    const auto silence = [](int, int) { return 0.0f; };
    const auto discard = [](int, int, float) {};
    const auto impulse = [](int, int i) { return i == 0 ? 0.4f : 0.0f; };
    process(plugin, 0.40, silence, discard);
    set(plugin, "Delay", 0.5f);
    float dryPeak = 0, repeat = 0;
    int repeatAt = 0;
    process(plugin, 0.52, impulse, [&](int ch, int i, float x)
    {
        if (ch == 0 && std::abs(i-latency) <= 1)
            dryPeak = std::max(dryPeak, std::abs(x));
        if (ch == 0 && i > latency+2000 && std::abs(x) > std::abs(repeat))
        {
            repeat = x;
            repeatAt = i;
        }
    });
    const int expectedRepeat = latency + static_cast<int>(rate*0.26);
    require(std::abs(dryPeak-0.40f) < 0.002f && std::abs(repeat-0.20f) < 0.002f
                && std::abs(repeatAt-expectedRepeat) <= 1,
            "Vocal delay tap failed. Peak " + juce::String(repeat, 5) + " at " + juce::String(repeatAt)
                + ", expected at " + juce::String(expectedRepeat) + ". Dry peak " + juce::String(dryPeak, 5));
    set(plugin, "Delay", 0);
    set(plugin, "Reverb", 0.8f);
    process(plugin, 0.15, silence, discard);
    float tail = 0;
    process(plugin, 0.30, impulse, [&](int ch, int i, float x)
    {
        if (ch == 0 && i > latency+1100)
            tail = std::max(tail, std::abs(x));
    });
    require(tail > 0.001f, "Reverb tail was silent");
    set(plugin, "Reverb", 0);
}

void checkBeatAnalysis()
{
    juce::TemporaryFile scratch(juce::String(".wav"));
    constexpr auto beatRate = 128.0;
    const auto samples = static_cast<int>(rate*12);
    juce::AudioBuffer<float> instrumental(1, samples);
    for (int i = 0; i < samples; ++i)
    {
        const auto phase = std::fmod(i/rate, 60/beatRate);
        const auto kick = 0.62f*static_cast<float>(std::exp(-phase*31))*tone(54, i);
        const auto chord = 0.16f*tone(130.813f, i) + 0.09f*tone(311.127f, i) + 0.11f*tone(391.995f, i);
        instrumental.setSample(0, i, kick+chord);
    }
    std::unique_ptr<juce::OutputStream> stream = scratch.getFile().createOutputStream();
    juce::WavAudioFormat wav;
    auto writer = wav.createWriterFor(stream, juce::AudioFormatWriterOptions().withSampleRate(rate)
        .withNumChannels(1).withBitsPerSample(24));
    require(writer && writer->writeFromAudioSampleBuffer(instrumental, 0, samples), "Couldn't write beat fixture");
    writer.reset();
    const auto analysis = kitchen::analyseBeat(scratch.getFile());
    std::cout << "Beat fixture: " << analysis.bpm << " BPM, tonic " << analysis.key
              << (analysis.minor ? " minor" : " major") << '\n';
    require(std::abs(analysis.bpm-beatRate) < 1.5, "Beat BPM was not detected");
    require(analysis.key == 0 && analysis.minor && !analysis.ambiguous, "C minor beat key was not detected confidently");
}
}

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    try
    {
        require(argc == 2, "Expected a VST3 or installed Audio Unit bundle path");
        const auto bundle = juce::String::fromUTF8(argv[1]);
        std::unique_ptr<juce::AudioPluginFormat> format;
       #if JUCE_PLUGINHOST_AU
        if (bundle.endsWithIgnoreCase(".component"))
            format = std::make_unique<juce::AudioUnitPluginFormat>();
       #endif
       #if JUCE_PLUGINHOST_VST3
        if (bundle.endsWithIgnoreCase(".vst3"))
            format = std::make_unique<juce::VST3PluginFormat>();
       #endif
        require(format != nullptr, "Unsupported plug-in format for this test executable");
        juce::OwnedArray<juce::PluginDescription> descriptions;
        format->findAllTypesForFile(descriptions, bundle);
        require(descriptions.size() == 1, "Expected one Chef's Touch effect");
        require(descriptions[0]->name == "Chef's Touch", "Unexpected plug-in display name: " + descriptions[0]->name);
        juce::String error;
        auto plugin = format->createInstanceFromDescription(*descriptions[0], rate, blockSize, error);
        require(plugin != nullptr, "Plug-in instantiation failed: " + error);
        plugin->prepareToPlay(rate, blockSize);
        const auto latency = plugin->getLatencySamples();
        require(latency == 1200, "Incorrect reported latency");

        set(*plugin, "Bypass", 1);
        float impulse = 0;
        float leaked = 0;
        process(*plugin, 0.12, [](int, int index) { return index == 0 ? 0.5f : 0.0f; },
                [&](int ch, int index, float sample)
                {
                    if (ch == 0 && index == latency) impulse = sample;
                    if (index != latency) leaked = std::max(leaked, std::abs(sample));
                });
        require(std::abs(impulse-0.5f) < 0.0001f && leaked < 0.0001f, "Bypass delay was not sample-aligned");
        set(*plugin, "Bypass", 0);
        set(*plugin, "Retune", 0);
        set(*plugin, "Humanize", 0);
        const auto corrected = correctedPitch(*plugin, 450);
        std::cout << "450 Hz Vocal input corrected to " << corrected << " Hz\n";
        require(std::abs(corrected-440) < 2.5, "Pitch correction did not converge on A4");
        set(*plugin, "Humanize", 1);
        const auto natural = correctedPitch(*plugin, 450);
        set(*plugin, "Tuning Style", 1);
        const auto locked = correctedPitch(*plugin, 450);
        std::cout << "At full Humanize: Natural " << natural << " Hz, Hard Lock " << locked << " Hz\n";
        require(std::abs(locked-440) + 2 < std::abs(natural-440), "Hard Lock did not strengthen correction");
        set(*plugin, "Tuning Style", 0);
        set(*plugin, "Humanize", 0);
        checkVocalEffects(*plugin, latency);
        checkBeatAnalysis();
        set(*plugin, "Tune Amount", 0);
        const auto quiet = rms(*plugin, 7600, 0.005f);
        set(*plugin, "Noise Floor", 0.76f);
        set(*plugin, "Noise Clean", 1);
        const auto clean = rms(*plugin, 7600, 0.005f);
        const auto vocal = rms(*plugin, 330, 0.2f);
        std::cout << "Quiet-band change: " << juce::Decibels::gainToDecibels(clean/quiet) << " dB\n";
        require(clean < quiet*0.18f && vocal > 0.125f, "Noise clean should reduce quiet hiss and preserve voiced level");
        set(*plugin, "Noise Clean", 0);
        set(*plugin, "Tune Amount", 1);

        set(*plugin, "Mode", 1);
        set(*plugin, "Stereo Width", 0);
        set(*plugin, "Peak Ceiling", 11.0f/12.0f);
        float peak = 0, stereoError = 0, previous = 0;
        process(*plugin, 0.55, [](int ch, int i)
                {
                    const auto wave = std::sin(juce::MathConstants<double>::pi*(0.5*i+0.25));
                    return (ch == 0 ? 1.55f : 0.55f)*static_cast<float>(wave);
                },
                [&](int ch, int i, float sample)
                {
                    if (i > static_cast<int>(rate*0.15))
                        peak = std::max(peak, std::abs(sample));
                    if (ch == 0)
                        previous = sample;
                    else
                        stereoError = std::max(stereoError, std::abs(previous-sample));
                });
        const auto intersamplePeak = peak*std::sqrt(2.0f);
        std::cout << "Master reconstructed sine peak: " << juce::Decibels::gainToDecibels(intersamplePeak) << " dB\n";
        require(intersamplePeak > 0.6f && intersamplePeak < 0.892f, "Master inter-sample peak ceiling failed");
        require(stereoError < 0.0001f, "Width at zero did not sum to mono");

        juce::MemoryBlock saved;
        plugin->getStateInformation(saved);
        require(saved.getSize() > 16, "Empty plug-in state");
        set(*plugin, "Mode", 0);
        set(*plugin, "Stereo Width", 1);
        plugin->setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
        require(parameter(*plugin, "Mode").getValue() > 0.99f, "Mode was not restored");
        require(parameter(*plugin, "Stereo Width").getValue() < 0.01f, "Width was not restored");
        {
            std::unique_ptr<juce::AudioProcessorEditor> editor(plugin->createEditorAndMakeActive());
            require(editor && editor->getWidth() > 0 && editor->getHeight() > 0, "Plug-in editor creation failed");
        }
        plugin->releaseResources();

        auto mono = plugin->getBusesLayout();
        mono.inputBuses.set(0, juce::AudioChannelSet::mono());
        mono.outputBuses.set(0, juce::AudioChannelSet::mono());
        require(plugin->setBusesLayout(mono), "Mono bus layout rejected");
        plugin->prepareToPlay(rate, blockSize);
        set(*plugin, "Mode", 0);

        process(*plugin, 0.1, [](int, int i) { return 0.2f*tone(220, i); }, [](int, int, float) {});
        plugin->releaseResources();
        std::cout << "PASS: plug-in discovery, stereo and mono processing, latency, bypass, Vocal pitch and effects, "
                     "Master width and ceiling, automation, state recall and editor creation.\n";
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}

