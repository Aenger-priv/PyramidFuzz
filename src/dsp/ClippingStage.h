#pragma once

#include "FeedbackClipper.h"

namespace pyramidfuzz::dsp
{
class ClippingStageA
{
public:
    void prepare(double rate, int, int channels) { clipper.prepare(rate, channels, false); }
    void reset() { clipper.reset(); }
    FeedbackClipper::Result processPort(int channel, SignalPort source) { return clipper.process(channel, source); }
private:
    FeedbackClipper clipper;
};

class ClippingStageB
{
public:
    void prepare(double rate, int, int channels)
    {
        coupling.prepare(rate, channels); clipper.prepare(rate, channels, true);
    }
    void reset() { coupling.reset(); clipper.reset(); }
    float processSample(int channel, float source, float mode)
    {
        const auto result = clipper.process(channel, coupling.begin(channel, source), mode);
        coupling.finish(channel, result.base);
        return static_cast<float>(result.output);
    }
private:
    InterstageCoupling coupling;
    FeedbackClipper clipper;
};
}
