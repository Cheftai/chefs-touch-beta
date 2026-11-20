#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <functional>

namespace kitchen
{
struct BeatResult
{
    double bpm = 0;
    int key = -1;
    bool minor = false;
    bool ambiguous = false;
    juce::String error;
};

BeatResult analyseBeat(const juce::File&, const std::function<bool()>& cancelled = [] { return false; });
}

