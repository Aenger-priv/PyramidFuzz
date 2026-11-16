#pragma once

#include "ClippingStage.h"
#include "InputVoicingStage.h"
#include "OutputStage.h"
#include "PreGainStage.h"
#include "SustainStage.h"
#include "ToneStack.h"

namespace pyramidfuzz::dsp
{
// One prepared circuit path for each quality rate. Only the selected path runs.
class NonlinearPath
{
public:
    void prepare(double rate, int size, int channels)
    {
        input.prepare(rate,size,channels); pre.prepare(rate,size,channels);
        sustain.prepare(rate,size,channels); a.prepare(rate,size,channels);
        b.prepare(rate,size,channels); tone.prepare(rate,size,channels);
        output.prepare(rate,size,channels);
    }
    void reset()
    {
        input.reset(); pre.reset(); sustain.reset(); a.reset(); b.reset(); tone.reset(); output.reset();
    }
    float process(int ch, float x, float gain, float voicing, float drive, float mode, float t, float h)
    {
        x=input.processSample(ch,x,gain,voicing);
        x=pre.processSample(ch,x);
        const auto port=sustain.beginSample(ch,x,drive,voicing);
        const auto clipped=a.processPort(ch,port);
        sustain.finishSample(ch,clipped.base);
        x=b.processSample(ch,static_cast<float>(clipped.output),mode);
        x=tone.processSample(ch,x,t,h);
        return output.processSample(ch,x);
    }
private:
    InputVoicingStage input;
    PreGainStage pre;
    SustainStage sustain;
    ClippingStageA a;
    ClippingStageB b;
    ToneStack tone;
    OutputStage output;
};
}
