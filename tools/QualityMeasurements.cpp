#include <JuceHeader.h>
#include "dsp/FuzzEngine.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>

using namespace pyramidfuzz::dsp;

int main(int argc, char** argv)
{
    juce::ScopedNoDenormals noDenormals;
    try
    {
        if (argc != 2) throw std::runtime_error("Usage: PyramidFuzzQualityMeasurements NEW_FOLDER");
        const auto folder = juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]);
        if (folder.exists() || !folder.createDirectory()) throw std::runtime_error("Use a new writable folder");
        std::ofstream bench(folder.getChildFile("cpu.csv").getFullPathName().toStdString());
        bench << std::setprecision(12) << "sample_rate,quality,channels,block_size,automation,mean_us,p50_us,p95_us,p99_us,max_us,deadline_us,latency\n";
        for (double rate : {44100.0,48000.0,96000.0})
            for (int quality = 0; quality < 3; ++quality)
                for (int channels : {1,2})
                    for (int size : {32,128,512})
                        for (int automation : {0,1})
                        {
                            FuzzParameters p; p.voicingMode=0; p.clippingMode=2; p.sustain=.9f;
                            p.oversamplingMode=quality;
                            FuzzEngine engine; engine.updateParameters(p); engine.prepare(rate,size,channels);
                            juce::AudioBuffer<float> buffer(channels,size);
                            std::vector<double> times;
                            for (int block=0; block<144; ++block)
                            {
                                for (int ch=0;ch<channels;++ch)
                                    for (int i=0;i<size;++i)
                                        buffer.setSample(ch,i,float(.2*std::sin(2*juce::MathConstants<double>::pi*110*(block*size+i)/rate)));
                                if (automation) { p.tone=float((block/12)%2); p.high=1-p.tone; }
                                const auto start=std::chrono::steady_clock::now();
                                engine.updateParameters(p); engine.processBlock(buffer);
                                const double us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count();
                                if (block>=16) times.push_back(us);
                            }
                            double sum=0;for (auto x:times) sum+=x;
                            std::sort(times.begin(),times.end());
                            bench<<rate<<','<<(2<<quality)<<','<<channels<<','<<size<<','<<automation<<','<<sum/times.size()<<','
                                 <<times[64]<<','<<times[121]<<','<<times[126]<<','<<times.back()<<','<<1e6*size/rate<<','<<engine.getLatencySamples()<<'\n';
                        }
        bench.close();
        std::cout<<"CPU grid complete"<<std::endl;
        // Analytic sine input at each host rate. Spectral samples are float32 WAV;
        // analysis masks genuine in-band harmonics and reports other energy.
        // 4x host rate / 8x internal is the independent 32x convergence reference.
        std::ofstream cases(folder.getChildFile("spectra.csv").getFullPathName().toStdString());
        cases<<"file,host_rate,reference_multiplier,quality,mode,tone,high,input_peak,frequency_bin,fft_size,latency\n";
        constexpr int n=16384;
        for (double hostRate:{44100.0,48000.0,96000.0})
            for (int multiplier:{1,4})
                for (int quality=0;quality<3;++quality)
                    for (int mode=0;mode<3;++mode)
                        for (int bin:{683,2423})
                        {
                            if (multiplier==4 && quality!=2) continue;
                            const double rate=hostRate*multiplier;
                            const double frequency=hostRate*bin/n;
                            FuzzParameters p;p.oversamplingMode=quality;p.clippingMode=mode;
                            p.sustain=1;p.tone=1;p.high=1;p.voicingMode=0;p.volumeDecibels=-11;
                            FuzzEngine engine;engine.updateParameters(p);engine.prepare(rate,128,1);
                            const int settle=int(std::ceil(.5*rate/128))*128;
                            juce::AudioBuffer<float> audio(1,n*multiplier),buffer(1,128);
                            for(int offset=0;offset<settle+n*multiplier;offset+=128)
                            {
                                for(int i=0;i<128;++i)
                                    buffer.setSample(0,i,float(.4*std::sin(2*juce::MathConstants<double>::pi*frequency*(offset+i)/rate)));
                                engine.updateParameters(p);engine.processBlock(buffer);
                                if(offset>=settle) audio.copyFrom(0,offset-settle,buffer,0,0,128);
                            }
                            const auto name=juce::String(int(hostRate))+"_ref"+juce::String(multiplier)+"_q"+juce::String(2<<quality)+"_mode"+juce::String(mode)+"_bin"+juce::String(bin)+".wav";
                            auto stream=folder.getChildFile(name).createOutputStream();
                            juce::WavAudioFormat format;
                            std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.get(),rate,1,32,{},0));
                            if(!writer) throw std::runtime_error("Cannot write spectrum WAV");
                            stream.release();writer->writeFromAudioSampleBuffer(audio,0,audio.getNumSamples());
                            cases<<name<<','<<hostRate<<','<<multiplier<<','<<(2<<quality)<<','<<mode<<",1,1,0.4,"<<bin<<','<<n<<','<<engine.getLatencySamples()<<'\n';
                        }
        std::cout<<"Quality measurements complete: "<<folder.getFullPathName()<<std::endl;
    }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
    return 0;
}
