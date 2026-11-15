#include <JuceHeader.h>
#include "dsp/FuzzEngine.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <stdexcept>

using namespace pyramidfuzz::dsp;

namespace
{
constexpr int blockSize = 128;
constexpr int spectrumOrder = 13;
constexpr int spectrumSize = 1 << spectrumOrder;
constexpr int fundamentalBin = 19;

struct Statistics
{
    double sum = 0.0, squares = 0.0, peak = 0.0;
    int64_t count = 0;
    void add(float x)
    {
        if (!std::isfinite(x)) throw std::runtime_error("Non-finite measurement");
        sum += x; squares += double(x) * x; peak = std::max(peak, std::abs(double(x))); ++count;
    }
    double rms() const { return count == 0 ? 0 : std::sqrt(squares / double(count)); }
    double dc() const { return count == 0 ? 0 : sum / double(count); }
};

// Fixed-parameter measurement adapter, NOT a second plugin engine. It exposes stage
// taps using production classes. Every render is checked against FuzzEngine below.
class TappedPath
{
public:
    TappedPath(double rate, int channels, const FuzzParameters& p) : parameters(p)
    {
        input.prepare(rate, blockSize, channels);
        tone.prepare(rate, blockSize, channels);
        output.prepare(rate, blockSize, channels);
        oversampling.prepare(channels, blockSize);
        oversampling.setMode(p.oversamplingMode);
        const auto factor = oversampling.getFactor();
        pre.prepare(rate * factor, blockSize * factor, channels);
        a.prepare(rate * factor, blockSize * factor, channels);
        b.prepare(rate * factor, blockSize * factor, channels);
    }

    void process(juce::AudioBuffer<float>& buffer, bool collect)
    {
        const auto tap = [this, collect](const char* name, float x)
        { if (collect) statistics[name].add(x); return x; };
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                buffer.setSample(ch, i, tap("input_voicing", input.processSample(ch,
                    tap("dry", buffer.getSample(ch, i)), dbToGain(parameters.inputDecibels), float(parameters.voicingMode))));
        oversampling.processBlock(buffer, [&](auto& up, int)
        {
            for (size_t ch = 0; ch < up.getNumChannels(); ++ch)
                for (size_t i = 0; i < up.getNumSamples(); ++i)
                {
                    auto x = tap("pre_gain", pre.processSample(int(ch), up.getSample(int(ch), int(i)), parameters.sustain));
                    x = tap("clip_a", a.processSample(int(ch), x, parameters.sustain));
                    x = tap("clip_b", b.processSample(int(ch), x, parameters.sustain, float(parameters.clippingMode)));
                    up.setSample(int(ch), int(i), clampAudio(x));
                }
        });
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                const auto x = tap("tone", tone.processSample(ch, tap("downsampled", buffer.getSample(ch, i)), parameters.tone, parameters.high));
                buffer.setSample(ch, i, tap("output", output.processSample(x, dbToGain(parameters.volumeDecibels))));
            }
    }
    std::map<std::string, Statistics> statistics;
private:
    FuzzParameters parameters;
    InputVoicingStage input;
    PreGainStage pre;
    ClippingStageA a;
    ClippingStageB b;
    ToneStack tone;
    OutputStage output;
    OversamplingEngine oversampling;
};

FuzzParameters defaults()
{
    // Host defaults in PluginProcessor.cpp; these differ from FuzzParameters' defaults.
    FuzzParameters p;
    p.sustain = 0.68f; p.tone = 0.34f; p.high = 0.22f;
    p.volumeDecibels = -11.0f; p.voicingMode = 1;
    return p;
}

std::ofstream csv(const juce::File& folder, const char* name)
{
    std::ofstream stream(folder.getChildFile(name).getFullPathName().toStdString());
    if (!stream) throw std::runtime_error("Cannot write measurement CSV");
    stream << std::setprecision(12);
    return stream;
}

void writeWav(const juce::File& file, const juce::AudioBuffer<float>& buffer, double rate)
{
    juce::WavAudioFormat format;
    auto stream = file.createOutputStream();
    if (!stream) throw std::runtime_error("Cannot create WAV");
    // 32-bit floating point preserves raw levels above unity without clipping.
    std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.get(), rate,
        static_cast<unsigned int>(buffer.getNumChannels()), 32, {}, 0));
    if (!writer) throw std::runtime_error("Cannot create WAV writer");
    stream.release();
    if (!writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples()))
        throw std::runtime_error("Cannot write WAV samples");
}

struct RenderResult
{
    juce::AudioBuffer<float> audio;
    std::map<std::string, Statistics> stages;
    int latency = 0;
    double tapError = 0;
};

RenderResult render(const juce::AudioBuffer<float>& dry, double rate, const FuzzParameters& p, int collectFrom = 0)
{
    FuzzEngine engine;
    engine.updateParameters(p);
    engine.prepare(rate, blockSize, dry.getNumChannels());
    TappedPath tapped(rate, dry.getNumChannels(), p);
    RenderResult result;
    result.audio.makeCopyOf(dry);
    result.latency = engine.getLatencySamples();
    for (int start = 0; start < dry.getNumSamples(); start += blockSize)
    {
        const int count = std::min(blockSize, dry.getNumSamples() - start);
        juce::AudioBuffer<float> block(result.audio.getArrayOfWritePointers(), dry.getNumChannels(), start, count);
        juce::AudioBuffer<float> copy;
        copy.makeCopyOf(block);
        engine.updateParameters(p);
        engine.processBlock(block);
        tapped.process(copy, start >= collectFrom);
        for (int ch = 0; ch < block.getNumChannels(); ++ch)
            for (int i = 0; i < count; ++i)
            {
                const auto value = block.getSample(ch, i);
                if (!std::isfinite(value)) throw std::runtime_error("Non-finite engine output");
                result.tapError = std::max(result.tapError, double(std::abs(value - copy.getSample(ch, i))));
            }
    }
    if (result.tapError > 1.0e-6) throw std::runtime_error("Stage adapter diverges from production engine");
    result.stages = std::move(tapped.statistics);
    return result;
}

void measureTone(const juce::File& folder)
{
    auto stream = csv(folder, "tone.csv");
    stream << "sample_rate,tone,high,frequency_hz,magnitude_db,phase_rad,impulse_tail_peak\n";
    constexpr int order = 16;
    constexpr int size = 1 << order;
    juce::dsp::FFT fft(order);
    for (const double rate : {44100.0, 48000.0, 96000.0})
        for (const float t : {0.0f, 0.25f, 0.5f, 0.75f, 1.0f})
            for (const float h : {0.0f, 0.25f, 0.5f, 0.75f, 1.0f})
            {
                ToneStack tone;
                tone.prepare(rate, blockSize, 1);
                std::vector<float> data(2 * size, 0);
                double tail = 0;
                for (int i = 0; i < size; ++i)
                {
                    data[size_t(i)] = tone.processSample(0, i == 0 ? 0.1f : 0.0f, t, h);
                    if (i >= size - 1024) tail = std::max(tail, double(std::abs(data[size_t(i)])));
                }
                fft.performRealOnlyForwardTransform(data.data());
                std::vector<int> bins;
                for (int i = 0; i <= 96; ++i)
                    bins.push_back(juce::roundToInt(20 * std::pow(1000.0, i / 96.0) * size / rate));
                for (double f : {100.0, 1000.0, 5000.0}) bins.push_back(juce::roundToInt(f * size / rate));
                std::sort(bins.begin(), bins.end()); bins.erase(std::unique(bins.begin(), bins.end()), bins.end());
                for (int bin : bins)
                {
                    const double re = data[size_t(2 * bin)], im = data[size_t(2 * bin + 1)];
                    stream << rate << ',' << t << ',' << h << ',' << bin * rate / size << ','
                           << 20 * std::log10(std::max(1.0e-12, std::hypot(re, im) / 0.1)) << ','
                           << std::atan2(im, re) << ',' << tail << '\n';
                }
            }
}

void measureToneDynamics(const juce::File& folder)
{
    auto stream = csv(folder, "tone-dynamics.csv");
    stream << "sample_rate,peak,max_sample_step,stereo_error,tail_peak\n";
    for (double rate : {44100.0, 48000.0, 96000.0})
    {
        ToneStack stack;
        stack.prepare(rate, blockSize, 2);
        juce::SmoothedValue<float> tone, high;
        resetSmoother(tone, rate, 0.03, 0.0f);
        resetSmoother(high, rate, 0.03, 0.0f);
        const float toneTargets[] = {0, 1, 1, 0, .25f, .75f, .5f, 1, 0};
        const float highTargets[] = {0, 0, 1, 1, .75f, .25f, .5f, 0, 1};
        double peak = 0, maxStep = 0, stereoError = 0, tailPeak = 0;
        float previous = 0;
        for (int i = 0; i < int(2 * rate); ++i)
        {
            const double time = i / rate;
            if (i % blockSize == 0 && time < 1.5)
            {
                const int index = std::min(8, int(time / .15));
                tone.setTargetValue(toneTargets[index]);
                high.setTargetValue(highTargets[index]);
            }
            const auto t = tone.getNextValue(), h = high.getNextValue();
            const double fade = std::clamp((1.5 - time) / .02, 0.0, 1.0);
            const auto input = float(fade * (.07 * std::sin(2 * juce::MathConstants<double>::pi * 110 * time)
                                            + .03 * std::sin(2 * juce::MathConstants<double>::pi * 5000 * time)));
            const float left = stack.processSample(0, input, t, h);
            const float right = stack.processSample(1, input, t, h);
            if (!std::isfinite(left) || !std::isfinite(right)) throw std::runtime_error("Non-finite automated tone output");
            peak = std::max(peak, double(std::abs(left)));
            maxStep = std::max(maxStep, double(std::abs(left - previous)));
            stereoError = std::max(stereoError, double(std::abs(left - right)));
            if (time > 1.9) tailPeak = std::max(tailPeak, double(std::abs(left)));
            previous = left;
        }
        stream << rate << ',' << peak << ',' << maxStep << ',' << stereoError << ',' << tailPeak << '\n';
    }
}

void measureGain(const juce::File& folder)
{
    auto stages = csv(folder, "stages.csv");
    stages << "case_id,sample_rate,oversampling,mode,voicing,sustain,input_peak_dbfs,stage,rms,peak,dc,sample_count,latency,tap_max_error\n";
    auto harmonics = csv(folder, "harmonics.csv");
    harmonics << "case_id,sample_rate,fundamental_hz,harmonic,amplitude_peak,dbfs\n";
    juce::dsp::FFT fft(spectrumOrder);
    int id = 0;
    const auto measure = [&](double rate, int quality, int mode, int voice, float sustain, float level)
    {
        auto p = defaults(); p.oversamplingMode = quality; p.clippingMode = mode; p.voicingMode = voice; p.sustain = sustain;
        juce::AudioBuffer<float> dry(1, 2 * spectrumSize);
        const double amplitude = std::pow(10.0, level / 20.0);
        for (int i = 0; i < dry.getNumSamples(); ++i)
            dry.setSample(0, i, float(amplitude * std::sin(2 * juce::MathConstants<double>::pi * fundamentalBin * i / spectrumSize)));
        const auto result = render(dry, rate, p, spectrumSize);
        for (const auto& entry : result.stages)
        {
            const auto& s = entry.second;
            stages << id << ',' << rate << ',' << (1 << (quality + 1)) << ',' << mode << ',' << voice << ',' << sustain << ',' << level << ',' << entry.first
                   << ',' << s.rms() << ',' << s.peak << ',' << s.dc() << ',' << s.count << ',' << result.latency << ',' << result.tapError << '\n';
        }
        std::vector<float> data(2 * spectrumSize, 0);
        std::copy_n(result.audio.getReadPointer(0, spectrumSize), spectrumSize, data.begin());
        fft.performRealOnlyForwardTransform(data.data());
        for (int n = 1; n <= 32; ++n)
        {
            const int bin = n * fundamentalBin;
            const auto amp = 2 * std::hypot(double(data[size_t(2 * bin)]), double(data[size_t(2 * bin + 1)])) / spectrumSize;
            harmonics << id << ',' << rate << ',' << rate * fundamentalBin / spectrumSize << ',' << n << ',' << amp << ',' << 20 * std::log10(std::max(1.0e-12, amp)) << '\n';
        }
        ++id;
    };
    for (int mode = 0; mode < 3; ++mode)
        for (int voice = 0; voice < 2; ++voice)
            for (float sustain : {0.0f, 0.68f, 1.0f})
                for (float level : {-48.0f, -36.0f, -24.0f, -12.0f, -6.0f}) measure(48000, 1, mode, voice, sustain, level);
    for (double rate : {44100.0, 48000.0, 96000.0})
        for (int quality = 0; quality < 3; ++quality)
            for (int mode = 0; mode < 3; ++mode) measure(rate, quality, mode, 1, 0.68f, -24.0f);
}

juce::AudioBuffer<float> syntheticGuitar(double rate)
{
    juce::AudioBuffer<float> dry(1, int(5 * rate)); dry.clear();
    // Deliberately synthetic plucks/chord, not a DI recording or a pedal reference.
    const double notes[] = {82.4069, 110.0, 146.8324, 220.0};
    for (int i = 0; i < int(4 * rate); ++i)
    {
        const double time = i / rate;
        double value = 0;
        for (int note = 0; note < 4; ++note)
        {
            const double elapsed = time - note * 0.65;
            if (elapsed < 0) continue;
            for (int harmonic = 1; harmonic <= 8; ++harmonic)
                value += (1 - std::exp(-elapsed * 1000)) * std::exp(-elapsed * (1.7 + harmonic * 0.12))
                    * std::sin(2 * juce::MathConstants<double>::pi * notes[note] * harmonic * elapsed) / harmonic;
        }
        const double endFade = std::clamp((4.0 - time) / 0.02, 0.0, 1.0);
        dry.setSample(0, i, float(value * endFade));
    }
    dry.applyGain(std::pow(10.0f, -18.0f / 20.0f) / dry.getMagnitude(0, dry.getNumSamples()));
    return dry;
}

void measureRenders(const juce::File& folder, const juce::StringArray& files)
{
    auto summary = csv(folder, "renders.csv");
    summary << "file,source,sample_rate,channels,oversampling,mode,voicing,rms,peak,dc,latency,tap_max_error\n";
    auto decay = csv(folder, "decay.csv");
    decay << "file,time_seconds,rms,peak,dc\n";
    juce::AudioFormatManager formats; formats.registerBasicFormats();
    for (int sourceIndex = -1; sourceIndex < files.size(); ++sourceIndex)
    {
        double rate = 48000;
        juce::AudioBuffer<float> dry;
        const auto source = sourceIndex < 0 ? juce::String("synthetic_plucks") : "di_" + juce::String(sourceIndex);
        if (sourceIndex < 0) dry = syntheticGuitar(rate);
        else
        {
            std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(juce::File(files[sourceIndex])));
            if (!reader || reader->numChannels < 1 || reader->numChannels > 2 || reader->lengthInSamples > 600 * reader->sampleRate)
                throw std::runtime_error("DI must be readable mono/stereo audio, at most ten minutes");
            rate = reader->sampleRate;
            dry.setSize(int(reader->numChannels), int(reader->lengthInSamples + int64_t(rate)));
            dry.clear();
            if (!reader->read(&dry, 0, int(reader->lengthInSamples), 0, true, true)) throw std::runtime_error("Cannot read DI");
        }
        writeWav(folder.getChildFile(source + "_dry.wav"), dry, rate);
        for (int quality = 0; quality < 3; ++quality)
            for (int mode = 0; mode < 3; ++mode)
                for (int voice = 0; voice < 2; ++voice)
                {
                    if (quality != 1 && voice != 1) continue;
                    auto p = defaults(); p.oversamplingMode = quality; p.clippingMode = mode; p.voicingMode = voice;
                    const auto result = render(dry, rate, p);
                    const auto name = source + "_" + juce::String(1 << (quality + 1)) + "x_mode" + juce::String(mode) + "_voice" + juce::String(voice) + ".wav";
                    writeWav(folder.getChildFile(name), result.audio, rate);
                    const auto& s = result.stages.at("output");
                    summary << name << ',' << source << ',' << rate << ',' << dry.getNumChannels() << ',' << (1 << (quality + 1)) << ',' << mode << ',' << voice
                            << ',' << s.rms() << ',' << s.peak << ',' << s.dc() << ',' << result.latency << ',' << result.tapError << '\n';
                    const int window = int(rate / 20);
                    for (int start = 0; start < result.audio.getNumSamples(); start += window)
                    {
                        Statistics stats;
                        for (int ch = 0; ch < result.audio.getNumChannels(); ++ch)
                            for (int i = start; i < std::min(start + window, result.audio.getNumSamples()); ++i) stats.add(result.audio.getSample(ch, i));
                        decay << name << ',' << start / rate << ',' << stats.rms() << ',' << stats.peak << ',' << stats.dc() << '\n';
                    }
                }
    }
}
}

int main(int argc, char** argv)
{
    juce::ScopedNoDenormals noDenormals;
    try
    {
        juce::String output;
        juce::StringArray di;
        bool toneOnly = false;
        for (int i = 1; i < argc; ++i)
        {
            const std::string arg(argv[i]);
            if (arg == "--output" && i + 1 < argc) output = argv[++i];
            else if (arg == "--di" && i + 1 < argc) di.add(argv[++i]);
            else if (arg == "--tone-only") toneOnly = true;
            else throw std::runtime_error("Usage: PyramidFuzzMeasurements --output NEW_FOLDER [--tone-only | --di WAV_OR_AIFF]...");
        }
        if (output.isEmpty()) throw std::runtime_error("--output is required");
        const auto folder = juce::File::getCurrentWorkingDirectory().getChildFile(output);
        if (folder.exists()) throw std::runtime_error("Output folder already exists; refusing to overwrite measurements");
        if (!folder.createDirectory()) throw std::runtime_error("Cannot create output folder");
        auto protocol = csv(folder, "protocol.json");
        const auto p = defaults();
        protocol << "{\n  \"block_size\": " << blockSize
                 << ",\n  \"defaults\": {\"input_db\": " << p.inputDecibels << ", \"sustain_normalized\": " << p.sustain
                 << ", \"tone\": " << p.tone << ", \"high\": " << p.high << ", \"volume_db\": " << p.volumeDecibels
                 << ", \"voicing\": " << p.voicingMode << ", \"clipping\": " << p.clippingMode << ", \"oversampling_index\": " << p.oversamplingMode
                 << "},\n  \"mode_labels\": [\"Off\", \"Silicon\", \"Germanium\"],\n  \"voicing_labels\": [\"Hi\", \"Lo\"],"
                 << "\n  \"tone_impulse_samples\": 65536,\n  \"tone_impulse_amplitude\": 0.1,"
                 << "\n  \"gain_fft_samples\": " << spectrumSize << ",\n  \"gain_fundamental_bin\": " << fundamentalBin
                 << ",\n  \"gain_settle_samples\": " << spectrumSize
                 << ",\n  \"synthetic_dry_peak_dbfs\": -18,\n  \"synthetic_seconds\": 5,\n  \"synthetic_signal_ends_seconds\": 4,"
                 << "\n  \"synthetic_end_fade_seconds\": 0.02,\n  \"decay_window_seconds\": 0.05,"
                 << "\n  \"di_resampling\": false,\n  \"di_normalization\": false,\n  \"di_appended_silence_seconds\": 1\n}\n";
        protocol.close();
        std::cout << "Measuring tone impulse grid..." << std::endl;
        measureTone(folder);
        measureToneDynamics(folder);
        if (!toneOnly)
        {
            std::cout << "Measuring gain, harmonics, and stage taps..." << std::endl;
            measureGain(folder);
            std::cout << "Rendering synthetic plucks and optional DI..." << std::endl;
            measureRenders(folder, di);
        }
        std::cout << "Measurements complete: " << folder.getFullPathName() << std::endl;
        return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
