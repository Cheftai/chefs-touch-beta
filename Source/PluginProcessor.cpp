#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
constexpr std::array<const char*, 26> ids {
    "mode", "key", "scale", "quality", "input", "output", "retune", "human",
    "low", "mid", "high", "threshold", "ratio", "deess", "room", "echo",
    "time", "drive", "mix", "width", "ceiling", "bypass", "tune", "denoise", "noiseFloor", "tuneStyle"
};
struct Recipe
{
    const char* name;
    std::array<float, 21> settings;
};
constexpr Recipe recipes[] {
    { "Vocal / House Seasoning", { 0,0,0,1, 0,0,55,.30f, -2,2,1, -19,2.5f,.35f,.10f,.06f, 260,.08f,1,1,-1 } },
    { "Vocal / Hard Tune",       { 0,0,1,1, 0,0,5,0, -3,2,2, -22,3,.45f,.04f,.12f, 185,.14f,1,1,-1 } },
    { "Vocal / Slow Simmer",     { 0,0,0,1, 0,0,135,.70f, -1,1,2, -16,2,.30f,.25f,.20f, 360,.06f,1,1,-1 } },
    { "Master / Clean Plate",    { 1,0,0,1, 0,0,55,.3f, 0,0,0, -15,1.5f,0,0,0, 260,0,1,1,-1 } },
    { "Master / Golden Finish",  { 1,0,0,1, 0,1,55,.3f, .8f,-.6f,1.1f, -18,1.65f,0,0,0, 260,.11f,1,1.13f,-1 } },
    { "Master / Hot Plate",      { 1,0,0,1, 2,2,55,.3f, 1,0,.8f, -20,2,0,0,0, 260,.18f,1,1.05f,-1.2f } },
    { "Vocal / Trap", { 0,0,0,1,0,0,5,0,-3,2,3,-23,3.5f,0.45f,0.06f,0.14f,187,0.16f,1,1,-1 } },
    { "Vocal / Pop", { 0,0,0,1,0,0,35,0.3f,-2,2,2.5f,-20,3,0.4f,0.12f,0.09f,250,0.08f,1,1,-1 } },
    { "Vocal / Rock", { 0,0,0,1,0,0,90,0.6f,-2,3,1,-21,4,0.3f,0.08f,0.04f,120,0.2f,1,1,-1 } },
    { "Vocal / R&B", { 0,0,0,1,0,0,65,0.55f,-1,1,2,-19,2.5f,0.4f,0.2f,0.16f,375,0.08f,1,1,-1 } },
    { "Vocal / Hip Hop", { 0,0,0,1,0,0,18,0.15f,-3,2,1,-22,3.5f,0.4f,0.04f,0.08f,210,0.14f,1,1,-1 } },
    { "Vocal / Acoustic", { 0,0,0,1,0,0,150,0.8f,-1,0.5f,1.5f,-17,2,0.25f,0.12f,0.03f,300,0.03f,1,1,-1 } },
    { "Vocal / EDM", { 0,0,0,1,0,0,8,0.05f,-3,2,3,-22,3.5f,0.45f,0.15f,0.18f,250,0.12f,1,1,-1 } },
    { "Vocal / Jazz Soul", { 0,0,0,1,0,0,190,0.9f,-0.5f,0.5f,1,-16,1.8f,0.2f,0.16f,0.02f,330,0.04f,1,1,-1 } },
    { "Master / Trap", { 1,0,0,1,0,0,55,0.3f,1,-0.5f,1,-18,1.8f,0,0,0,260,0.12f,1,1.06f,-1 } },
    { "Master / Pop", { 1,0,0,1,0,0,55,0.3f,0.5f,-0.4f,1,-17,1.6f,0,0,0,260,0.07f,1,1.1f,-1 } },
    { "Master / Rock", { 1,0,0,1,0,0,55,0.3f,0.6f,0.5f,0.4f,-18,1.8f,0,0,0,260,0.12f,1,1.04f,-1 } },
    { "Master / R&B", { 1,0,0,1,0,0,55,0.3f,0.8f,-0.5f,0.8f,-17,1.5f,0,0,0,260,0.06f,1,1.08f,-1 } },
    { "Master / Hip Hop", { 1,0,0,1,0,0,55,0.3f,1,0,0.5f,-18,1.7f,0,0,0,260,0.1f,1,1.03f,-1 } },
    { "Master / Acoustic", { 1,0,0,1,0,0,55,0.3f,0,0.2f,0.6f,-15,1.3f,0,0,0,260,0.02f,1,1.03f,-1 } },
    { "Master / EDM", { 1,0,0,1,0,0,55,0.3f,1,-0.5f,1.2f,-19,2,0,0,0,260,0.12f,1,1.12f,-1 } },
    { "Master / Jazz Soul", { 1,0,0,1,0,0,55,0.3f,0.3f,0,0.4f,-14,1.25f,0,0,0,260,0.03f,1,1.02f,-1 } }
};
}

ChefsTouchAudioProcessor::ChefsTouchAudioProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      state(*this, nullptr, "ChefsTouchState", createParameterLayout())
{
    for (size_t i = 0; i < ids.size(); ++i)
        values[i] = state.getRawParameterValue(ids[i]);
}

ChefsTouchAudioProcessor::State::ParameterLayout ChefsTouchAudioProcessor::createParameterLayout()
{
    State::ParameterLayout layout;
    const auto choice = [&](const char* id, const char* name, juce::StringArray options, int index)
    {
        layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID(id, 1), name, options, index));
    };
    const auto number = [&](const char* id, const char* name, float lo, float hi, float step, float init, const char* unit)
    {
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(id, 1), name,
            juce::NormalisableRange<float>(lo, hi, step), init, juce::AudioParameterFloatAttributes().withLabel(unit)));
    };
    const auto proportion = [&](const char* id, const char* name, float init)
    {
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(id, 1), name,
            juce::NormalisableRange<float>(0, 1, 0.01f), init,
            juce::AudioParameterFloatAttributes().withLabel("%")
                .withStringFromValueFunction([](float v, int) { return juce::String(juce::roundToInt(100*v)); })
                .withValueFromStringFunction([](const juce::String& v) { return v.getFloatValue()*0.01f; })));
    };
    choice("mode", "Mode", {"Vocal", "Master"}, 0);
    choice("key", "Key", {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"}, 0);
    choice("scale", "Scale", {"Chromatic", "Major", "Minor"}, 0);
    choice("quality", "Quality", {"Standard", "High"}, 1);
    number("input", "Input", -24, 24, .1f, 0, "dB");
    number("output", "Output", -24, 24, .1f, 0, "dB");
    number("retune", "Retune", 3, 250, 1, 55, "ms");
    proportion("human", "Humanize", .30f);
    number("low", "Low Shelf", -12, 12, .1f, 0, "dB");
    number("mid", "Mid Bell", -12, 12, .1f, 0, "dB");
    number("high", "High Shelf", -12, 12, .1f, 0, "dB");
    number("threshold", "Comp Threshold", -48, 0, .1f, -18, "dB");
    number("ratio", "Comp Ratio", 1, 12, .1f, 1, ":1");
    proportion("deess", "De-esser", 0);
    proportion("room", "Reverb", 0);
    proportion("echo", "Delay", 0);
    number("time", "Delay Time", 45, 800, 1, 260, "ms");
    proportion("drive", "Saturation", 0);
    proportion("mix", "Wet / Dry", 1);
    number("width", "Stereo Width", 0, 2, .01f, 1, "x");
    number("ceiling", "Peak Ceiling", -12, 0, .1f, -1, "dBTP");
    proportion("tune", "Tune Amount", 1);
    proportion("denoise", "Noise Clean", 0);
    number("noiseFloor", "Noise Floor", -78, -28, 1, -52, "dB");
    choice("tuneStyle", "Tuning Style", {"Natural", "Hard Lock"}, 0);
    choice("delaySync", "Delay Sync", {"Free ms", "1/16", "1/8", "1/8 dotted", "1/4", "1/4 dotted", "1/2"}, 0);
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("manualPitch",1),"Manual pitch timeline",false));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("bypass", 1), "Bypass", false));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("animation", 1), "Animate Chef", true,
        juce::AudioParameterBoolAttributes().withAutomatable(false)));
    return layout;
}

bool ChefsTouchAudioProcessor::isBusesLayoutSupported(const BusesLayout& layout) const
{
    const auto in = layout.getMainInputChannelSet();
    return in == layout.getMainOutputChannelSet()
        && (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo());
}

void ChefsTouchAudioProcessor::prepareToPlay(double rate, int)
{
    processing.prepare(rate, getTotalNumInputChannels());
    loudness.prepare(rate);
    setLatencySamples(processing.latency());
    reset();
}

void ChefsTouchAudioProcessor::reset()
{
    processing.reset();
    loudness.reset();
    lufsMomentary=lufsShortTerm=lufsIntegrated=-100;
    meterIn = meterL = meterR = detectedPitch = reduction = 0;
    mouthLevel = vocalConfidence = vocalCents = 0;
    meterFrame.fetch_add(1, std::memory_order_release);
}

void ChefsTouchAudioProcessor::processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer&)
{
    render(audio, values[21]->load(std::memory_order_relaxed) >= 0.5f);
}

void ChefsTouchAudioProcessor::processBlockBypassed(juce::AudioBuffer<float>& audio, juce::MidiBuffer&)
{
    render(audio, true);
}

void ChefsTouchAudioProcessor::render(juce::AudioBuffer<float>& audio, bool bypass)
{
    if (audio.getNumSamples() == 0 || audio.getNumChannels() == 0)
        return;
    const auto v = [&](size_t index) { return values[index]->load(std::memory_order_relaxed); };
    auto delayMs = v(16);
    double bpm = 0;
    double timelineSeconds=-1;
    if (auto* hostPlayHead = getPlayHead())
        if (const auto position = hostPlayHead->getPosition())
        {
            if(const auto seconds=position->getTimeInSeconds()) timelineSeconds=*seconds;
            if (const auto tempo = position->getBpm(); tempo && std::isfinite(*tempo) && *tempo >= 20 && *tempo <= 400)
                bpm = *tempo;
        }
    const auto sync = static_cast<int>(state.getRawParameterValue("delaySync")->load(std::memory_order_relaxed));
    constexpr float beats[] { 0, .25f, .5f, .75f, 1, 1.5f, 2 };
    if (bpm > 0 && sync > 0 && sync < 7)
        delayMs = static_cast<float>(60000.0 / bpm) * beats[sync];
    hostTempo.store(static_cast<float>(bpm), std::memory_order_relaxed);
    syncedDelayMs.store(delayMs, std::memory_order_relaxed);
    kitchen::Controls controls {
        static_cast<int>(v(0)), static_cast<int>(v(1)), static_cast<int>(v(2)), static_cast<int>(v(3)),
        v(4),v(5),v(6),v(7),v(8),v(9),v(10),v(11),v(12),v(13),v(14),v(15),delayMs,v(17),v(18),v(19),v(20),
        v(22),v(23),v(24),v(25) >= 0.5f
    };
    if(state.getRawParameterValue("manualPitch")->load()>=0.5f && timelineSeconds>=0)
        controls.manualNote=pitchEdits.target(timelineSeconds);
    const auto result = processing.process(audio, controls, bypass);
    if (resetLoudness.exchange(false, std::memory_order_relaxed)) loudness.reset();
    loudness.process(audio);
    lufsMomentary.store(loudness.momentary, std::memory_order_relaxed);
    lufsShortTerm.store(loudness.shortTerm, std::memory_order_relaxed);
    lufsIntegrated.store(loudness.integrated, std::memory_order_relaxed);
    const auto fall = std::exp(-static_cast<float>(audio.getNumSamples()) /
                               static_cast<float>(0.18 * getSampleRate()));
    const auto holdPeak = [fall](std::atomic<float>& dest, float peak)
    {
        dest.store(std::max(peak, fall*dest.load(std::memory_order_relaxed)), std::memory_order_relaxed);
    };
    holdPeak(meterIn, result.input);
    holdPeak(meterL, result.left);
    holdPeak(meterR, result.right);
    holdPeak(reduction, result.reduction);
    detectedPitch.store(result.pitch, std::memory_order_relaxed);
    const auto mouthFall = std::exp(-static_cast<float>(audio.getNumSamples()) / static_cast<float>(0.045*getSampleRate()));
    mouthLevel.store(!bypass && controls.mode == 0 ? std::max(result.input, mouthFall*mouthLevel.load(std::memory_order_relaxed)) : 0,
                     std::memory_order_relaxed);
    vocalConfidence.store(result.pitchConfidence, std::memory_order_relaxed);
    vocalCents.store(result.targetCents, std::memory_order_relaxed);
    meterFrame.fetch_add(1, std::memory_order_release);
}

juce::AudioProcessorParameter* ChefsTouchAudioProcessor::getBypassParameter() const { return state.getParameter("bypass"); }

const juce::StringArray& ChefsTouchAudioProcessor::presets()
{
    static const auto names = []
    {
        juce::StringArray result;
        for (const auto& preset : recipes)
            result.add(preset.name);
        return result;
    }();
    return names;
}

void ChefsTouchAudioProcessor::loadPreset(int index)
{
    if (!juce::isPositiveAndBelow(index, static_cast<int>(std::size(recipes))))
        return;
    for (size_t i = 0; i < recipes[index].settings.size(); ++i)
    {
        if (i == 3) // Quality belongs to settings.
            continue;
        auto* parameter = state.getParameter(ids[i]);
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(parameter->convertTo0to1(recipes[index].settings[i]));
        parameter->endChangeGesture();
    }
    for (const auto* id : { "tune", "denoise", "noiseFloor" })
    {
        auto* parameter = state.getParameter(id);
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(parameter->getDefaultValue());
        parameter->endChangeGesture();
    }
    auto* style = state.getParameter("tuneStyle");
    style->beginChangeGesture();
    style->setValueNotifyingHost((index == 1 || index == 6 || index == 12) ? 1.0f : 0.0f);
    style->endChangeGesture();
    selectedPreset.store(index);
}

void ChefsTouchAudioProcessor::getStateInformation(juce::MemoryBlock& data)
{
    auto copy = state.copyState();
    copy.setProperty("preset", selectedPreset.load(), nullptr);
    copy.setProperty("pitchEdits", getPitchEdits(), nullptr);
    if (const auto xml = copy.createXml())
        copyXmlToBinary(*xml, data);
}

void ChefsTouchAudioProcessor::setStateInformation(const void* data, int bytes)
{
    if (auto xml = getXmlFromBinary(data, bytes); xml && xml->hasTagName(state.state.getType()))
    {
        auto restored = juce::ValueTree::fromXml(*xml);
        const auto index = static_cast<int>(restored.getProperty("preset", -1));
        selectedPreset.store(juce::isPositiveAndBelow(index, presets().size()) ? index : -1);
        state.replaceState(restored);
        setPitchEdits(restored.getProperty("pitchEdits").toString());
    }
}

bool ChefsTouchAudioProcessor::restorePreset(const juce::MemoryBlock& data)
{
    const auto xml = getXmlFromBinary(data.getData(), static_cast<int>(data.getSize()));
    if (xml == nullptr || !xml->hasTagName(state.state.getType()))
        return false;
    const auto incoming = juce::ValueTree::fromXml(*xml);
    setPitchEdits(incoming.getProperty("pitchEdits").toString());
    for (auto* parameter : AudioProcessor::getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter);
        if (ranged == nullptr || ranged->paramID == "bypass" || ranged->paramID == "animation")
            continue;
        const auto entry = incoming.getChildWithProperty("id", ranged->paramID);
        if (!entry.isValid() || !entry.hasProperty("value"))
            continue;
        const auto value = static_cast<float>(entry["value"]);
        if (!std::isfinite(value))
            continue;
        ranged->beginChangeGesture();
        ranged->setValueNotifyingHost(ranged->convertTo0to1(value));
        ranged->endChangeGesture();
    }
    const auto index = static_cast<int>(incoming.getProperty("preset", -1));
    selectedPreset.store(juce::isPositiveAndBelow(index, presets().size()) ? index : -1);
    return true;
}

juce::AudioProcessorEditor* ChefsTouchAudioProcessor::createEditor() { return new ChefsTouchEditor(*this); }
juce::String ChefsTouchAudioProcessor::setPitchEdits(const juce::String& text)
{
    const juce::ScopedLock lock(pitchTextLock);
    const auto error=pitchEdits.apply(text);
    if(error.isEmpty()) pitchEditText=text;
    return error;
}
juce::String ChefsTouchAudioProcessor::getPitchEdits() const
{
    const juce::ScopedLock lock(pitchTextLock);
    return pitchEditText;
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ChefsTouchAudioProcessor; }


