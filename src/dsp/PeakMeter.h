#pragma once
#include <atomic>
#include <algorithm>
#include <cmath>

namespace pyramidfuzz::dsp
{
// Block-peak envelope: immediate attack, 400 ms exponential release, one-second
// overload hold. Audio owns the counters; the editor reads atomic snapshots.
class PeakMeter
{
    static_assert(std::atomic<float>::is_always_lock_free && std::atomic<bool>::is_always_lock_free, "Meters require lock-free atomic snapshots");
public:
    void prepare(double rate) { sampleRate=rate; reset(); }
    void reset() { envelope.store(0,std::memory_order_relaxed); clipped.store(false,std::memory_order_relaxed); holdSamples=0; }
    void push(float peak,int frames)
    {
        if(frames<=0) return;
        peak=std::isfinite(peak)?std::max(0.0f,peak):0.0f;
        const float release=static_cast<float>(std::exp(-frames/(sampleRate*.4)));
        envelope.store(std::max(peak,envelope.load(std::memory_order_relaxed)*release),std::memory_order_relaxed);
        holdSamples=peak>=1.0f?static_cast<int>(sampleRate):std::max(0,holdSamples-frames);
        clipped.store(holdSamples>0,std::memory_order_relaxed);
    }
    float level() const noexcept { return envelope.load(std::memory_order_relaxed); }
    bool overload() const noexcept { return clipped.load(std::memory_order_relaxed); }
private:
    double sampleRate=48000;
    int holdSamples=0;
    std::atomic<float> envelope {0};
    std::atomic<bool> clipped {false};
};
}
