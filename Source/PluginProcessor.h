
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "DSP/Processing.h"

class ChefsTouchAudioProcessor final : public juce::AudioProcessor
{
public:
    using State = juce::AudioProcessorValueTreeState;
    ChefsTouchAudioProcessor();
    ~ChefsTouchAudioProcessor() override = default;

    void prepareToPlay(double sampleRate,
                       int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;

    void processBlock(juce::AudioBuffer<float>&,
                      juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    using AudioProcessor::processBlockBypassed;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override
    {
        return "Chef's Touch";
    }

    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }

    double getTailLengthSeconds() const override
    {
        return 8.0;
    }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override
    {
        return {};
    }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    bool restorePreset(const juce::MemoryBlock&);
    juce::AudioProcessorParameter* getBypassParameter() const override;
    static const juce::StringArray& presets();
    void loadPreset(int index);

    State& getParameters() { return state; }
    static State::ParameterLayout createParameterLayout();

    std::atomic<int> selectedPreset { -1 };
    std::atomic<float> meterIn {}, meterL {}, meterR {}, detectedPitch {}, reduction {};
    std::atomic<unsigned> meterFrame {};
    std::atomic<float> mouthLevel {}, vocalConfidence {}, vocalCents {};
    std::atomic<float> hostTempo {}, syncedDelayMs {};

private:
    State state;
    void render(juce::AudioBuffer<float>&, bool);
    kitchen::Processing processing;
    std::array<const std::atomic<float>*, 26> values {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(
        ChefsTouchAudioProcessor)
};
