#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cmath>

namespace kitchen
{
// Mono/stereo BS.1770 K weighting. Integrated gating uses 0.1 LU bins.
// A measurement tool for mixing; not a certified broadcast instrument.
class Loudness
{
    struct Biquad
    {
        double b0=1, b1=0, b2=0, a1=0, a2=0, z1=0, z2=0;
        double tick(double x)
        {
            const double y=b0*x+z1;
            z1=b1*x-a1*y+z2; z2=b2*x-a2*y;
            return y;
        }
    };
public:
    void prepare(double rate)
    {
        quantum = std::max(1, static_cast<int>(std::round(rate*0.1)));
        for (auto& channel : filters)
        {
            const double k=std::tan(juce::MathConstants<double>::pi*1681.974450955533/rate);
            const double q=0.7071752369554196, vh=std::pow(10.0,3.999843853973347/20.0);
            const double vb=std::pow(vh,0.4996667741545416), d=1+k/q+k*k;
            channel[0]={(vh+vb*k/q+k*k)/d,2*(k*k-vh)/d,(vh-vb*k/q+k*k)/d,
                         2*(k*k-1)/d,(1-k/q+k*k)/d,0,0};
            const double hp=std::tan(juce::MathConstants<double>::pi*38.13547087602444/rate);
            const double hq=0.5003270373238773, hd=1+hp/hq+hp*hp;
            channel[1]={1,-2,1,2*(hp*hp-1)/hd,(1-hp/hq+hp*hp)/hd,0,0};
        }
        reset();
    }
    void reset()
    {
        for (auto& channel:filters) for(auto& f:channel) f.z1=f.z2=0;
        energies={}; histogramEnergy={}; histogramCount={};
        count=position=filled=0; energy=0;
        momentary=shortTerm=integrated=-100;
    }
    void process(const juce::AudioBuffer<float>& audio)
    {
        for(int i=0;i<audio.getNumSamples();++i)
        {
            for(int ch=0;ch<std::min(2,audio.getNumChannels());++ch)
            {
                const auto y=filters[static_cast<size_t>(ch)][1].tick(
                    filters[static_cast<size_t>(ch)][0].tick(audio.getSample(ch,i)));
                energy+=y*y;
            }
            if(++count!=quantum) continue;
            energies[static_cast<size_t>(position)]=energy/static_cast<double>(count);
            energy=0; count=0; position=(position+1)%30; filled=std::min(30,filled+1);
            const auto recent=[&](int n)
            {
                double sum=0;
                for(int j=0;j<n;++j) sum+=energies[static_cast<size_t>((position+29-j)%30)];
                return sum/static_cast<double>(n);
            };
            if(filled>=4)
            {
                const double block=recent(4);
                momentary=toLufs(block);
                if(momentary>=-70)
                {
                    const int bin=juce::jlimit(0,999,static_cast<int>((momentary+70)*10));
                    histogramEnergy[static_cast<size_t>(bin)]+=block;
                    ++histogramCount[static_cast<size_t>(bin)];
                }
                double sum=0; uint64_t blocks=0;
                for(size_t j=0;j<histogramEnergy.size();++j)
                { sum+=histogramEnergy[j]; blocks+=histogramCount[j]; }
                if(blocks>0)
                {
                    const auto gate=toLufs(sum/static_cast<double>(blocks))-10;
                    sum=0; blocks=0;
                    for(size_t j=0;j<histogramEnergy.size();++j)
                        if(-70+0.1*(static_cast<double>(j)+0.5)>=gate)
                        { sum+=histogramEnergy[j]; blocks+=histogramCount[j]; }
                    integrated=blocks>0 ? toLufs(sum/static_cast<double>(blocks)) : -100;
                }
            }
            if(filled>=30) shortTerm=toLufs(recent(30));
        }
    }
    float momentary=-100, shortTerm=-100, integrated=-100;
private:
    static float toLufs(double meanSquare)
    { return meanSquare>1.0e-10 ? static_cast<float>(-0.691+10*std::log10(meanSquare)) : -100; }
    std::array<std::array<Biquad,2>,2> filters;
    std::array<double,30> energies {};
    std::array<double,1000> histogramEnergy {};
    std::array<uint64_t,1000> histogramCount {};
    int quantum=4800,count=0,position=0,filled=0;
    double energy=0;
};
}
