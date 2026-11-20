#include "PluginEditor.h"
#include <BinaryData.h>

namespace
{
const juce::Colour gold { 0xffedb751 }, paper { 0xffffebc4 }, soft { 0xffbea99f };
const juce::Colour ink { 0xff110d10 }, panel { 0xff21181b }, tread { 0xff43272e }, red { 0xffb5293e };

void labelStyle(juce::Label& label, const juce::String& title, float size, juce::Colour colour)
{
    label.setText(title, juce::dontSendNotification);
    label.setFont(juce::Font(juce::FontOptions(size)));
    label.setColour(juce::Label::textColourId, colour);
}
}

ChefsTouchEditor::Theme::Theme()
{
    setColour(juce::ComboBox::backgroundColourId, panel);
    setColour(juce::ComboBox::outlineColourId, tread);
    setColour(juce::ComboBox::textColourId, paper);
    setColour(juce::ComboBox::arrowColourId, gold);
    setColour(juce::PopupMenu::backgroundColourId, panel);
    setColour(juce::PopupMenu::textColourId, paper);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, red);
    setColour(juce::TextButton::buttonColourId, panel);
    setColour(juce::TextButton::buttonOnColourId, red);
    setColour(juce::TextButton::textColourOffId, paper);
    setColour(juce::TextButton::textColourOnId, paper);
    setColour(juce::ToggleButton::textColourId, paper);
    setColour(juce::ToggleButton::tickColourId, gold);
    setColour(juce::Slider::textBoxTextColourId, paper);
    setColour(juce::Slider::textBoxBackgroundColourId, ink);
    setColour(juce::Slider::textBoxOutlineColourId, tread);
}

void ChefsTouchEditor::Theme::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
                                              float value, float start, float end, juce::Slider&)
{
    const float radius = 0.43f*static_cast<float>(std::min(w,h));
    if (radius < 2)
        return;
    const auto cx = static_cast<float>(x) + 0.5f*static_cast<float>(w);
    const auto cy = static_cast<float>(y) + 0.5f*static_cast<float>(h);
    const auto angle = start + value*(end-start);
    const juce::Rectangle<float> bounds(cx-radius, cy-radius, 2*radius, 2*radius);
    juce::Path arc;
    arc.addCentredArc(cx, cy, radius, radius, 0, start, end, true);
    g.setColour(tread);
    g.strokePath(arc, juce::PathStrokeType(3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (value > 0)
    {
        arc.clear();
        arc.addCentredArc(cx, cy, radius, radius, 0, start, angle, true);
        g.setColour(gold);
        g.strokePath(arc, juce::PathStrokeType(3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    g.setGradientFill(juce::ColourGradient(tread, cx, bounds.getY(), ink, cx, bounds.getBottom(), false));
    g.fillEllipse(bounds.reduced(5));
    g.setColour(paper);
    g.drawLine(cx + std::sin(angle)*radius*0.4f, cy - std::cos(angle)*radius*0.4f,
               cx + std::sin(angle)*radius*0.7f, cy - std::cos(angle)*radius*0.7f, 2.2f);
}

ChefsTouchEditor::Knob::Knob(ChefsTouchAudioProcessor::State& state, const char* id, const char* caption, int visibility)
    : pages(visibility), attachment(state, id, slider)
{
    labelStyle(label, caption, 12, soft);
    label.setJustificationType(juce::Justification::centred);
    slider.setName(caption);
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 69, 21);
    slider.setTitle(caption);
    slider.setDoubleClickReturnValue(true, slider.getValue());
}

ChefsTouchEditor::ChefsTouchEditor(ChefsTouchAudioProcessor& owner)
    : AudioProcessorEditor(owner), processor(owner)
{
    setLookAndFeel(&theme);
    const auto bindChoice = [&](juce::ComboBox& box, const char* id)
    {
        auto* parameter = dynamic_cast<juce::AudioParameterChoice*>(processor.getParameters().getParameter(id));
        box.addItemList(parameter->choices, 1);
        box.setTitle(parameter->getName(40));
        addAndMakeVisible(box);
        choices.push_back(std::make_unique<ChefsTouchAudioProcessor::State::ComboBoxAttachment>(processor.getParameters(), id, box));
    };
    bindChoice(mode, "mode");
    bindChoice(key, "key");
    bindChoice(scale, "scale");
    bindChoice(quality, "quality");
    bindChoice(tuneStyle, "tuneStyle");
    bindChoice(delaySync, "delaySync");
    delaySync.setTooltip("Delay note division follows DAW tempo. If tempo is unavailable, Delay time in ms is used.");
    tuneStyle.setTooltip("Hard Lock snaps rapidly to the selected scale. Natural leaves more of the original pitch movement.");
    presets.addItemList(ChefsTouchAudioProcessor::presets(), 1);
    presets.setTextWhenNothingSelected("Factory presets");
    presets.setTitle("Factory presets");
    presets.onChange = [this] { processor.loadPreset(presets.getSelectedItemIndex()); };
    bypassAttachment = std::make_unique<ChefsTouchAudioProcessor::State::ButtonAttachment>(processor.getParameters(), "bypass", bypass);
    animationAttachment = std::make_unique<ChefsTouchAudioProcessor::State::ButtonAttachment>(processor.getParameters(), "animation", animate);
    settings.onClick = [this] { selectPage(page == 1 ? 0 : 1); };
    learn.onClick = [this] { selectPage(page == 2 ? 0 : 2); };
    animate.onClick = [this] { repaint(stage); };
    savePreset.onClick = [this] { choosePresetFile(true); };
    openPreset.onClick = [this] { choosePresetFile(false); };
    importBeat.onClick = [this] { chooseBeat(); };
    applyKey.onClick = [this] { applyBeatKey(); };
    applyKey.setEnabled(false);
    coachTopic.addItemList({ "Tuning and gain", "Noise and sibilance", "EQ and dynamics", "Space and loudness" }, 1);
    coachTopic.setSelectedId(1, juce::dontSendNotification);
    coachTopic.onChange = [this] { updateCoach(); };

    for (juce::Component* component : std::initializer_list<juce::Component*> {
             &presets,&settings,&learn,&bypass,&animate,&heading,&subheading,&coach,&qualityLabel,&latencyLabel,
             &keyLabel,&scaleLabel,&savePreset,&openPreset,&presetStatus,&importBeat,&applyKey,&beatStatus,&coachTopic,&coachStatus })
        addAndMakeVisible(component);

    labelStyle(heading, "Chef's Touch", 37, paper);
    heading.setFont(juce::Font(juce::FontOptions(37).withStyle("Bold")));
    labelStyle(subheading, "", 16, gold);
    labelStyle(keyLabel, "Key", 12, soft);
    labelStyle(scaleLabel, "Scale", 12, soft);
    labelStyle(qualityLabel, "Processing quality", 14, paper);
    labelStyle(latencyLabel, "", 14, soft);
    labelStyle(presetStatus, "", 14, soft);
    labelStyle(coachStatus, "", 13, gold);
    labelStyle(beatStatus, "Load the instrumental audio file", 13, soft);
    coach.setMultiLine(true, true);
    coach.setReadOnly(true);
    coach.setScrollbarsShown(true);
    coach.setFont(juce::Font(juce::FontOptions(15)));
    coach.setColour(juce::TextEditor::textColourId, paper);
    coach.setColour(juce::TextEditor::backgroundColourId, ink);
    coach.setColour(juce::TextEditor::outlineColourId, tread);
    coach.setIndents(12, 10);

    const auto knob = [&](const char* id, const char* name, int visibility)
    {
        auto item = std::make_unique<Knob>(processor.getParameters(), id, name, visibility);
        auto* parameter = processor.getParameters().getParameter(id);
        item->slider.setDoubleClickReturnValue(true, parameter->convertFrom0to1(parameter->getDefaultValue()));
        addAndMakeVisible(item->slider);
        addAndMakeVisible(item->label);
        knobs.push_back(std::move(item));
    };
    knob("input", "Input", 3); knob("output", "Output", 3);
    knob("retune", "Retune", 1); knob("human", "Humanize", 1);
    knob("low", "Low shelf", 3); knob("mid", "Mid bell", 3); knob("high", "High shelf", 3);
    knob("threshold", "Threshold", 3); knob("ratio", "Ratio", 3);
    knob("deess", "De-esser", 1); knob("room", "Reverb", 1);
    knob("echo", "Delay", 1); knob("time", "Delay time", 1);
    knob("drive", "Saturation", 3); knob("mix", "Wet / dry", 1);
    knob("tune", "Tune amount", 1);
    knob("denoise", "Noise clean", 1); knob("noiseFloor", "Noise floor", 1);
    knob("width", "Stereo width", 2); knob("ceiling", "Peak ceiling", 2);

    const auto sheet = juce::ImageCache::getFromMemory(BinaryData::ChefSauceSheet_png,
                                                     BinaryData::ChefSauceSheet_pngSize);
    if (sheet.isValid())
    {
        const int width = sheet.getWidth()/2, height = sheet.getHeight()/2;
        for (int i = 0; i < 4; ++i)
            chefFrames[static_cast<size_t>(i)] = sheet.getClippedImage({(i%2)*width, (i/2)*height, width, height});
    }
    setResizable(true, true);
    setResizeLimits(840, 570, 1500, 980);
    setSize(1010, 670);
    selectPage(0);
    startTimerHz(30);
}

ChefsTouchEditor::~ChefsTouchEditor()
{
    stopTimer();
    cancelBeat->store(true);
    beatWorker.removeAllJobs(true, -1);
    presetChooser.reset();
    setLookAndFeel(nullptr);
}

void ChefsTouchEditor::selectPage(int wanted)
{
    page = wanted;
    if (page == 2)
        updateCoach();
    settings.setToggleState(page == 1, juce::dontSendNotification);
    learn.setToggleState(page == 2, juce::dontSendNotification);
    resized();
    repaint();
}

void ChefsTouchEditor::resized()
{
    auto layout = getLocalBounds().reduced(22);
    auto top = layout.removeFromTop(56);
    heading.setBounds(top.removeFromLeft(270));
    auto nav = top.reduced(0, 11);
    mode.setBounds(nav.removeFromLeft(96).reduced(3, 0));
    presets.setBounds(nav.removeFromLeft(std::min(240, nav.getWidth()-222)).reduced(3, 0));
    learn.setBounds(nav.removeFromLeft(64).reduced(3, 0));
    settings.setBounds(nav.removeFromLeft(80).reduced(3, 0));
    bypass.setBounds(nav.removeFromLeft(78));
    layout.removeFromTop(13);
    stage = layout.removeFromLeft(juce::roundToInt(static_cast<float>(layout.getWidth())*0.34f));
    meterBox = stage.withTrimmedTop(stage.getHeight()-106).reduced(14, 5);
    layout.removeFromLeft(13);
    controls = layout;
    auto inside = controls.reduced(16);
    auto strip = inside.removeFromTop(32);
    const bool vocal = lastMode == 0;
    const bool showKeys = page == 0 && vocal;
    subheading.setText(page == 1 ? "Settings" : page == 2 ? "Mix coach" : vocal ? "Vocal chain" : "Mastering chain",
                      juce::dontSendNotification);
    subheading.setBounds(strip.removeFromLeft(showKeys ? 118 : 155));
    keyLabel.setBounds(strip.removeFromLeft(30));
    key.setBounds(strip.removeFromLeft(62).reduced(3, 2));
    scaleLabel.setBounds(strip.removeFromLeft(44));
    scale.setBounds(strip.removeFromLeft(103).reduced(3, 2));
    tuneStyle.setBounds(strip.removeFromLeft(106).reduced(3, 2));
    for (auto* c : std::initializer_list<juce::Component*> { &key, &scale, &keyLabel, &scaleLabel, &tuneStyle })
        c->setVisible(showKeys);
    for (auto* c : std::initializer_list<juce::Component*> { &importBeat, &applyKey, &beatStatus })
        c->setVisible(showKeys);
    if (showKeys)
    {
        auto beatRow = inside.removeFromTop(32);
        importBeat.setBounds(beatRow.removeFromLeft(113).reduced(2));
        applyKey.setBounds(beatRow.removeFromLeft(70).reduced(2));
        beatStatus.setBounds(beatRow);
    }
    inside.removeFromTop(12);

    qualityLabel.setVisible(page == 1);
    quality.setVisible(page == 1);
    animate.setVisible(page == 1);
    delaySync.setVisible(page == 1);
    latencyLabel.setVisible(page == 1);
    savePreset.setVisible(page == 1);
    openPreset.setVisible(page == 1);
    presetStatus.setVisible(page == 1);
    coach.setVisible(page == 2);
    coachTopic.setVisible(page == 2);
    coachStatus.setVisible(page == 2);
    if (page == 1)
    {
        auto row = inside.removeFromTop(34);
        qualityLabel.setBounds(row.removeFromLeft(165));
        quality.setBounds(row.removeFromLeft(145).reduced(2));
        animate.setBounds(inside.removeFromTop(36));
        delaySync.setBounds(inside.removeFromTop(32).removeFromLeft(180).reduced(2));
        latencyLabel.setBounds(inside.removeFromTop(36));
        auto files = inside.removeFromTop(38);
        savePreset.setBounds(files.removeFromLeft(134).reduced(3));
        openPreset.setBounds(files.removeFromLeft(134).reduced(3));
        presetStatus.setBounds(inside.removeFromTop(36));
    }
    else if (page == 2)
    {
        coachTopic.setBounds(inside.removeFromTop(32).removeFromLeft(205).reduced(2));
        coachStatus.setBounds(inside.removeFromTop(26));
        coach.setBounds(inside.reduced(2, 7));
    }

    const int columns = vocal ? 6 : 4;
    const int rows = 3;
    int slot = 0;
    for (auto& item : knobs)
    {
        const bool visible = page == 0 && (item->pages & (vocal ? 1 : 2));
        item->slider.setVisible(visible);
        item->label.setVisible(visible);
        if (!visible)
            continue;
        const int col = slot % columns, row = slot / columns;
        auto cell = juce::Rectangle<int>(inside.getX() + col*inside.getWidth()/columns,
                                        inside.getY() + row*inside.getHeight()/rows,
                                        inside.getWidth()/columns, inside.getHeight()/rows).reduced(3, 4);
        item->label.setBounds(cell.removeFromTop(22));
        item->slider.setBounds(cell.withSizeKeepingCentre(cell.getWidth(), std::min(108, cell.getHeight())));
        ++slot;
    }
}

void ChefsTouchEditor::chooseBeat()
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    beatChooser = std::make_unique<juce::FileChooser>("Choose an instrumental for BPM and key",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), formats.getWildcardForAllFormats());
    beatChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safe = juce::Component::SafePointer(this)](const juce::FileChooser& chooser)
        {
            const auto file = chooser.getResult();
            if (safe == nullptr || file == juce::File{})
                return;
            safe->importBeat.setEnabled(false);
            safe->applyKey.setEnabled(false);
            safe->beatStatus.setText("Analyzing beat…", juce::dontSendNotification);
            safe->beatWorker.addJob(std::function<void()>([safe, file, cancel = safe->cancelBeat]
            {
                const auto result = kitchen::analyseBeat(file, [cancel] { return cancel->load(); });
                if (cancel->load())
                    return;
                juce::MessageManager::callAsync([safe, result]
                {
                    if (safe == nullptr)
                        return;
                    safe->beat = result;
                    safe->importBeat.setEnabled(true);
                    safe->applyKey.setEnabled(result.key >= 0);
                    const auto tempoText = result.bpm > 0 ? juce::String(result.bpm, 1) + " BPM" : "BPM unclear";
                    const auto keyText = result.key < 0 ? "Key unclear" :
                        safe->key.getItemText(result.key) + (result.minor ? " minor" : " major")
                            + (result.ambiguous ? " (check)" : "");
                    safe->beatStatus.setText(result.error.isNotEmpty() ? result.error : tempoText + "  /  " + keyText,
                                             juce::dontSendNotification);
                });
            }));
        });
}

void ChefsTouchEditor::applyBeatKey()
{
    if (beat.key < 0)
        return;
    key.setSelectedId(beat.key+1, juce::sendNotificationSync);
    scale.setSelectedId(beat.minor ? 3 : 2, juce::sendNotificationSync);
}

void ChefsTouchEditor::choosePresetFile(bool save)
{
    const auto defaultLocation = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
    presetChooser = std::make_unique<juce::FileChooser>(save ? "Save Chef's Touch preset" : "Open Chef's Touch preset",
        save ? defaultLocation.getChildFile("Chefs Touch.chefpreset") : defaultLocation, "*.chefpreset");
    const int modeFlags = juce::FileBrowserComponent::canSelectFiles
        | (save ? juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting
                : juce::FileBrowserComponent::openMode);
    presetChooser->launchAsync(modeFlags, [safe = juce::Component::SafePointer(this), save](const juce::FileChooser& chooser)
    {
        const auto file = chooser.getResult();
        if (safe == nullptr || file == juce::File{})
            return;
        juce::MemoryBlock data;
        bool success;
        if (save)
        {
            safe->processor.getStateInformation(data);
            success = file.withFileExtension("chefpreset").replaceWithData(data.getData(), data.getSize());
        }
        else
            success = file.getSize() <= 65536 && file.loadFileAsData(data) && safe->processor.restorePreset(data);
        safe->presetStatus.setText(success ? (save ? "Preset saved" : "Preset loaded")
                                            : (save ? "Couldn't save preset" : "Couldn't open preset"),
                                   juce::dontSendNotification);
    });
}

void ChefsTouchEditor::paint(juce::Graphics& g)
{
    g.fillAll(ink);
    // Dark kitchen tile, brass grout and a stainless range hood.
    for (int y = 4; y < getHeight(); y += 42)
        for (int x = (y / 42 % 2) * -42; x < getWidth(); x += 84)
        {
            g.setColour(juce::Colour(0xff292327));
            g.fillRoundedRectangle(static_cast<float>(x+2), static_cast<float>(y+2), 80.0f, 38.0f, 3.0f);
        }
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff51474a), 0, 0,
                                         ink, 0, 90, false));
    g.fillRect(8, 8, getWidth()-16, 72);
    // Small flame tongues keep the perimeter clear of controls.
    for (int x = 8; x < getWidth()-8; x += 18)
    {
        const float fx = static_cast<float>(x);
        const float height = 8.0f + 7.0f * std::abs(std::sin(fx * 0.13f));
        juce::Path flame;
        flame.startNewSubPath(fx-7, static_cast<float>(getHeight()-3));
        flame.quadraticTo(fx-8, getHeight()-height, fx+2, getHeight()-height-5);
        flame.quadraticTo(fx-1, getHeight()-9.0f, fx+8, getHeight()-3.0f);
        flame.closeSubPath();
        g.setGradientFill(juce::ColourGradient(gold, fx, getHeight()-3.0f,
                          juce::Colour(0xffe44420), fx, getHeight()-height, false));
        g.fillPath(flame);
    }
    g.setColour(gold.withAlpha(0.7f));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(3), 10, 2);
    g.setColour(panel);
    g.fillRoundedRectangle(controls.toFloat(), 12);
    g.setColour(tread);
    g.drawRoundedRectangle(controls.toFloat().reduced(.5f), 12, 1);
}

void ChefsTouchEditor::paintOverChildren(juce::Graphics& g)
{
    drawChef(g);
    drawMeters(g);
}

void ChefsTouchEditor::drawChef(juce::Graphics& g)
{
    juce::Graphics::ScopedSaveState save(g);
    g.reduceClipRegion(stage);
    const auto frame = stage.toFloat();
    const auto center = frame.getCentreX();
    const auto base = static_cast<float>(meterBox.getY()) - 10;
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff671d28), center, frame.getY()+frame.getHeight()*0.43f,
                                           panel, frame.getX(), frame.getY(), true));
    g.fillRoundedRectangle(frame, 12);
    const auto animated = animate.getToggleState();
    const auto stageFrame = animated ? mouth > 0.12f ? 1 : celebration > 32 ? 2 : celebration > 0 ? 3 : 0 : 0;
    const auto& image = chefFrames[static_cast<size_t>(stageFrame)];
    if (!image.isValid())
        return;

    const auto dest = juce::Rectangle<float>(frame.getX()+8, frame.getY()+12, frame.getWidth()-16, base-frame.getY()-12);
    const auto fitted = juce::RectanglePlacement(juce::RectanglePlacement::xMid | juce::RectanglePlacement::yBottom)
                            .appliedTo(image.getBounds().toFloat(), dest);
    g.setImageResamplingQuality(juce::Graphics::mediumResamplingQuality);
    g.drawImage(image, fitted);
    if (!animated || stageFrame != 3)
        return;

    const auto fire = std::sin(juce::MathConstants<float>::pi*(1.0f-static_cast<float>(celebration)/33.0f));
    const auto width = fitted.getWidth();
    const auto surface = fitted.getY()+fitted.getHeight()*0.88f;
    for (int i = 0; i < 9; ++i)
    {
        const auto index = static_cast<float>(i)-4;
        const auto x = fitted.getCentreX() + index*width*0.043f;
        const auto bend = 0.024f*width*std::sin(index+static_cast<float>(tick)*0.56f);
        const auto height = width*fire*(0.12f + 0.25f*std::abs(std::sin(index*1.2f+static_cast<float>(tick)*0.27f)));
        juce::Path flame;
        flame.startNewSubPath(x-width*0.029f, surface);
        flame.quadraticTo(x-width*0.065f, surface-height*0.39f, x+bend, surface-height);
        flame.quadraticTo(x+bend*0.45f, surface-height*0.36f, x+width*0.038f, surface);
        flame.closeSubPath();
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xffffeb81), x, surface,
                                               juce::Colour(0xffe84622), x, surface-height, false));
        g.fillPath(flame);
    }
}
void ChefsTouchEditor::drawMeters(juce::Graphics& g)
{
    auto region = meterBox;
    g.setColour(ink.withAlpha(0.85f));
    g.fillRoundedRectangle(region.toFloat(), 8);
    region.reduce(9, 5);
    constexpr const char* names[] { "IN", "L", "R" };
    g.setFont(juce::Font(juce::FontOptions(11)));
    for (size_t i = 0; i < levels.size(); ++i)
    {
        auto line = region.removeFromTop(18);
        const auto db = juce::Decibels::gainToDecibels(levels[i], -60.0f);
        g.setColour(soft);
        g.drawText(names[i], line.removeFromLeft(23), juce::Justification::centredLeft);
        g.drawText(db <= -60 ? "-inf" : juce::String(db, 1), line.removeFromRight(39), juce::Justification::centredRight);
        auto bar = line.reduced(2, 5).toFloat();
        g.setColour(tread);
        g.fillRoundedRectangle(bar, 3);
        g.setColour(db > -1 ? juce::Colour(0xfff66758) : gold);
        g.fillRoundedRectangle(bar.withWidth(bar.getWidth()*juce::jlimit(0.0f, 1.0f, (db+60)/60)), 3);
    }
    g.setColour(paper);
    auto info = region.removeFromTop(27);
    juce::String pitchText = "Pitch --";
    if (pitch > 0)
    {
        const auto midi = 69.0f + 12.0f*std::log2(pitch/440.0f);
        const auto note = juce::roundToInt(midi);
        const auto cents = juce::roundToInt(100*(midi-static_cast<float>(note)));
        pitchText = juce::MidiMessage::getMidiNoteName(note, true, true, 4) + "  "
            + (cents > 0 ? "+" : "") + juce::String(cents) + "c";
    }
    g.drawText(lastMode == 0 ? pitchText : "Master output",
               info, juce::Justification::centredLeft);
    g.drawText("GR  " + juce::String(gr, 1) + " dB", info, juce::Justification::centredRight);
}

void ChefsTouchEditor::updateCoach()
{
    const auto mean = [&](size_t i) { return coachStats[i] / static_cast<float>(std::max(1, coachSamples)); };
    juce::String status;
    if (bypass.getToggleState())
        status = "Turn off Bypass to hear the settings.";
    else if (mean(0) < 0.005f)
        status = "Loop a loud and a quiet phrase to compare settings.";
    else if (mean(0) > 0.84f)
        status = "Input is hot. Lower the track feeding this plug-in by 6 dB.";
    else if (mean(3) > 6)
        status = "GR is high. Raise Threshold by 3 dB and compare.";
    else if (lastMode == 0 && mean(4) < 0.35f)
        status = "Try a sustained note on an isolated vocal track.";
    else
        status = "Match the loudness before comparing with Bypass.";
    coachStatus.setText(status, juce::dontSendNotification);

    const auto topic = coachTopic.getSelectedId();
    juce::String guide;
    if (lastMode == 0)
    {
        if (topic == 1)
            guide = "1. Start with Input and Output at 0 dB. Loop your loudest line and adjust the level feeding the plug-in until IN peaks are roughly -12 to -6 dBFS. If the incoming recording is clipped, lower or re-record it at the source.\n\n"
                    "2. Click Analyze beat, choose the instrumental, then Use key. If the result sounds wrong on several notes, confirm the key by ear. Try Chromatic while you check.\n\n"
                    "3. For natural tuning try Retune 45–70 ms, Tune Amount 75–90%, and Humanize 25–45%. If held notes wobble, shorten Retune by about 10 ms. If slides disappear, add 10–20% Humanize.\n\n"
                    "4. For audible snap select Hard Lock, Retune 3–10 ms, Tune Amount 100%, Humanize 0–10%. Back off Tune Amount by 10% if note transitions sound too abrupt.";
        else if (topic == 2)
            guide = "1. Loop a phrase with a pause. Set Noise Clean to about 40% and Noise Floor near -55 dB. Raise Noise Floor in 3 dB steps until the room tone drops in the pause.\n\n"
                    "2. If quiet word endings or breaths vanish, lower Noise Floor by 3–6 dB. If the pause still sounds noisy and words remain clear, raise Noise Clean to 50–65%.\n\n"
                    "3. Loop a line with several S sounds. Start De-esser at 25–35%. Add 10% if S still leaps out. Back off 10% if words sound lispy. A quieter room or cleaner recording helps with bleed under the voice.";
        else if (topic == 3)
            guide = "1. Start EQ flat. Try Low shelf -2 dB if the vocal feels boomy, Mid bell +1 to +2 dB if words get lost, or High shelf +1 dB for air. Change one band at a time.\n\n"
                    "2. Set Ratio around 2.5:1 and Threshold around -18 dB. Lower Threshold until louder words show 2–5 dB of GR. If the delivery loses impact, raise Threshold 3 dB or lower Ratio toward 2:1.\n\n"
                    "3. Try Saturation around 5–12%. If the high end becomes rough, reduce it by 5%. Match Output to the bypassed level by ear.";
        else
            guide = "1. Start Reverb at 8–12% and Delay at 4–8%. Raise one control at a time. If the lyric becomes hard to understand, reduce each by about 4 percentage points.\n\n"
                    "2. To match the beat, quarter-note delay is 60,000 divided by BPM in ms. For an eighth note, halve that number. At 120 BPM try 250 ms.\n\n"
                    "3. Start Wet / dry at 100% when tuning. Reducing it mixes back the original pitch. Toggle Bypass and use Output in 1 dB steps to match loudness.";
    }
    else if (topic <= 2)
        guide = "MASTER STARTING POINT\n\n"
                "1. Play the loudest section. Start Input and Output at 0 dB, Stereo Width at 1.00x, EQ flat and Saturation at 0%. Trim the level feeding the plug-in if the original mix is clipping.\n\n"
                "2. Try Ratio 1.5:1 and Threshold -16 dB. Adjust Threshold until loud sections show about 1–3 dB of GR. If the chorus shrinks, raise Threshold 2–3 dB.\n\n"
                "3. Start Peak Ceiling at -1 dBTP. Listen for harshness on kicks and cymbals as you add Output 1 dB at a time. If they lose punch, reduce Output 1–2 dB.";
    else if (topic == 3)
        guide = "1. Start EQ flat. Try changes of 0.5–1 dB per band. Reduce Low shelf if the mix is heavy, cut Mid bell slightly if it feels crowded, or raise High shelf slightly if the mix is dull.\n\n"
                "2. Start Ratio at 1.5:1 and Threshold at -16 dB. Aim for 1–3 dB of GR on the loudest passage. Raise Threshold if the drums lose punch.\n\n"
                "3. Add 5–8% Saturation if you want more density. Reduce it if cymbals or consonants get gritty at matched loudness.";
    else
        guide = "1. Start Stereo Width at 1.00x. Try 1.05–1.15x for a little more spread. Temporarily set Width to 0x to check mono and listen for disappearing instruments.\n\n"
                "2. Start Peak Ceiling at -1 dBTP. Raise Output in 1 dB steps until you reach the loudness you need or hear the drums soften. If the latter happens first, lower Output 1–2 dB.\n\n"
                "3. Compare with Bypass at matched loudness. Check a quiet section too.";
    if (coach.getText() != guide)
        coach.setText(guide, false);
}
void ChefsTouchEditor::timerCallback()
{
    const auto newMode = static_cast<int>(processor.getParameters().getRawParameterValue("mode")->load());
    if (newMode != lastMode)
    {
        lastMode = newMode;
        coachStats = {};
        coachSamples = 0;
        resized();
        repaint();
    }
    const int selected = processor.selectedPreset.load();
    if (selected != lastPreset)
    {
        presets.setSelectedItemIndex(selected, juce::dontSendNotification);
        lastPreset = selected;
    }

    const auto meterFrame = processor.meterFrame.load(std::memory_order_acquire);
    idleMeterFrames = meterFrame == lastMeterFrame ? std::min(8, idleMeterFrames+1) : 0;
    lastMeterFrame = meterFrame;
    if (idleMeterFrames < 8)
    {
        levels = { processor.meterIn.load(), processor.meterL.load(), processor.meterR.load() };
        pitch = processor.detectedPitch.load();
        gr = processor.reduction.load();
    }
    else
    {
        for (auto& level : levels)
            level *= 0.80f;
        gr *= 0.80f;
        pitch = 0;
    }
    tick = (tick+1) % 12000;
    const bool vocalActive = lastMode == 0 && idleMeterFrames < 8 && !bypass.getToggleState();
    const auto vocalLevel = vocalActive ? processor.mouthLevel.load(std::memory_order_relaxed) : 0.0f;
    const auto mouthTarget = juce::jlimit(0.0f, 1.0f, (std::sqrt(vocalLevel)-0.07f)*2.8f);
    mouth += (mouthTarget > mouth ? 0.84f : 0.62f)*(mouthTarget-mouth);
    const bool withinTarget = vocalActive && vocalLevel > 0.025f
        && processor.vocalConfidence.load(std::memory_order_relaxed) >= 0.9f
        && std::abs(processor.vocalCents.load(std::memory_order_relaxed)) <= 12.0f
        && levels[0] < 0.98f && std::max(levels[1], levels[2]) < 0.98f;
    celebration = vocalActive && animate.getToggleState() ? std::max(0, celebration-1) : 0;
    if (!withinTarget)
    {
        inTuneFrames = 0;
        celebrationArmed = true;
    }
    else if (celebration == 0 && celebrationArmed && animate.getToggleState() && ++inTuneFrames >= 14)
    {
        celebration = 53; // Sauce first, then the pan flare.
        celebrationArmed = false;
    }
    const std::array<float, 5> now { levels[0], levels[1], levels[2], gr, pitch > 0 ? 1.0f : 0.0f };
    if (coachSamples >= 90)
    {
        for (auto& value : coachStats)
            value *= 89.0f/90.0f;
        coachSamples = 89;
    }
    ++coachSamples;
    for (size_t i = 0; i < now.size(); ++i)
        coachStats[i] += now[i];

    if (page == 1 && tick % 12 == 0)
    {
        const int samples = processor.getLatencySamples();
        const auto rate = processor.getSampleRate();
        latencyLabel.setText(rate > 0 ? "Reported latency  " + juce::String(samples) + " samples / "
                                    + juce::String(1000*samples/rate, 1) + " ms" : "Latency available during playback",
                            juce::dontSendNotification);
    }
    if (page == 2 && tick % 15 == 0)
        updateCoach();
    repaint(stage);
}

