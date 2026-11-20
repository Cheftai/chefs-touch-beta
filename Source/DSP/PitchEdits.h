#pragma once
#include <juce_core/juce_core.h>
#include <array>
#include <atomic>
#include <cstdlib>
#include <cmath>

namespace kitchen
{
// A small manual timeline in DAW seconds. Updates never lock the audio callback.
class PitchEdits
{
public:
    juce::String apply(const juce::String& text)
    {
        struct Region { float start,end,note; };
        std::array<Region,64> parsed {};
        int count=0;
        for (const auto& line:juce::StringArray::fromLines(text))
        {
            if(line.trim().isEmpty() || line.trimStart().startsWithChar('#')) continue;
            const auto fields=juce::StringArray::fromTokens(line,",","");
            if(fields.size()!=3 || count==64) return "Use up to 64 rows: start seconds, end seconds, MIDI note.";
            const auto number=[](const juce::String& field,float& result)
            {
                const auto value=field.trim();
                if(value.isEmpty()) return false;
                const auto* begin=value.toRawUTF8();
                char* end=nullptr;
                result=std::strtof(begin,&end);
                return end!=begin && *end=='\0' && std::isfinite(result);
            };
            auto& r=parsed[static_cast<size_t>(count)];
            if(!number(fields[0],r.start) || !number(fields[1],r.end) || !number(fields[2],r.note)
                || r.start<0 || r.end<=r.start || r.note<36 || r.note>84
                || std::floor(r.note)!=r.note || (count>0 && r.start<parsed[static_cast<size_t>(count-1)].end))
                return "Rows must be ordered, non-overlapping; end > start; MIDI note 36-84.";
            ++count;
        }
        version.fetch_add(1,std::memory_order_acq_rel);
        for(int i=0;i<count;++i)
        {
            auto& dest=regions[static_cast<size_t>(i)]; const auto& src=parsed[static_cast<size_t>(i)];
            dest[0].store(src.start);dest[1].store(src.end);dest[2].store(src.note);
        }
        size.store(count);
        version.fetch_add(1,std::memory_order_release);
        return {};
    }
    int target(double seconds) const
    {
        const auto before=version.load(std::memory_order_acquire);
        if(before%2!=0 || !std::isfinite(seconds)) return -1;
        int note=-1;
        const auto count=size.load();
        for(int i=0;i<count;++i)
        {
            const auto& r=regions[static_cast<size_t>(i)];
            const auto start=r[0].load(),end=r[1].load(),pitch=r[2].load();
            if(seconds>=start && seconds<end) { note=static_cast<int>(pitch);break; }
        }
        return version.load(std::memory_order_acquire)==before ? note : -1;
    }
private:
    std::array<std::array<std::atomic<float>,3>,64> regions {};
    std::atomic<int> size {};
    std::atomic<unsigned> version {};
};
}
