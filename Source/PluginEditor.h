#pragma once
#include "PluginProcessor.h"
#include "DSP/BeatAnalysis.h"

class ChefsTouchEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ChefsTouchEditor(ChefsTouchAudioProcessor&);
    ~ChefsTouchEditor() override;
    void paint(juce::Graphics&) override;
    void paintOverChildren(juce::Graphics&) override;
    void resized() override;

private:
    struct Theme final : juce::LookAndFeel_V4
    {
        Theme();
        void drawRotarySlider(juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    };
    struct Knob
    {
        Knob(ChefsTouchAudioProcessor::State&, const char* id, const char* caption, int visibility);
        juce::Label label;
        juce::Slider slider;
        int pages;
        ChefsTouchAudioProcessor::State::SliderAttachment attachment;
    };
    void timerCallback() override;
    void selectPage(int);
    void drawChef(juce::Graphics&);
    void drawMeters(juce::Graphics&);
    void updateCoach();
    void choosePresetFile(bool save);
    void chooseBeat();
    void applyBeatKey();

    ChefsTouchAudioProcessor& processor;
    Theme theme;
    juce::ComboBox mode, presets, key, scale, quality, tuneStyle, delaySync;
    juce::TextButton settings { "Settings" }, learn { "Learn" };
    juce::TextButton savePreset { "Save preset…" }, openPreset { "Open preset…" };
    juce::TextButton importBeat { "Analyze beat…" }, applyKey { "Use key" };
    juce::ToggleButton bypass { "Bypass" }, animate { "Animate chef" };
    juce::Label heading, subheading, qualityLabel, latencyLabel, keyLabel, scaleLabel;
    juce::ComboBox coachTopic;
    juce::TextEditor coach;
    juce::Label presetStatus, coachStatus;
    juce::Label beatStatus;
    std::unique_ptr<juce::FileChooser> presetChooser;
    std::unique_ptr<juce::FileChooser> beatChooser;
    juce::ThreadPool beatWorker { 1 };
    std::shared_ptr<std::atomic<bool>> cancelBeat = std::make_shared<std::atomic<bool>>(false);
    kitchen::BeatResult beat;
    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<ChefsTouchAudioProcessor::State::ComboBoxAttachment>> choices;
    std::unique_ptr<ChefsTouchAudioProcessor::State::ButtonAttachment> bypassAttachment, animationAttachment;
    std::array<juce::Image, 4> chefFrames;
    juce::Rectangle<int> stage, controls, meterBox;
    int page = 0, lastMode = 0, lastPreset = -2, tick = 0, celebration = 0, inTuneFrames = 0;
    bool celebrationArmed = true;
    float pitch = 0, gr = 0, mouth = 0;
    std::array<float, 3> levels {};
    unsigned lastMeterFrame = 0;
    int idleMeterFrames = 0;
    std::array<float, 5> coachStats {};
    int coachSamples = 0;
};

