#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>

namespace { thread_local bool trackAllocations=false; thread_local size_t allocations=0; }
void* operator new(std::size_t n) { if(trackAllocations) ++allocations; if(auto* p=std::malloc(std::max(n,size_t(1)))) return p; throw std::bad_alloc(); }
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p,std::size_t) noexcept { std::free(p); }
void operator delete[](void* p,std::size_t) noexcept { std::free(p); }
void* operator new(std::size_t n,std::align_val_t a) { if(trackAllocations) ++allocations; void* p=nullptr; if(posix_memalign(&p,static_cast<size_t>(a),std::max(n,size_t(1)))==0) return p; throw std::bad_alloc(); }
void* operator new[](std::size_t n,std::align_val_t a) { return ::operator new(n,a); }
void operator delete(void* p,std::align_val_t) noexcept { std::free(p); }
void operator delete[](void* p,std::align_val_t) noexcept { std::free(p); }

namespace
{
int failures=0;
void check(bool ok,const char* text){if(!ok){++failures;std::cerr<<text<<'\n';}}
void set(PyramidFuzzAudioProcessor& p,const char* id,float value)
{
    auto* parameter=p.getValueTreeState().getParameter(id);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
juce::AudioProcessor::BusesLayout layout(int inputs,int outputs)
{
    juce::AudioProcessor::BusesLayout result;
    result.inputBuses.add(juce::AudioChannelSet::canonicalChannelSet(inputs));
    result.outputBuses.add(juce::AudioChannelSet::canonicalChannelSet(outputs));
    return result;
}
void signal(juce::AudioBuffer<float>& b,int offset,double rate)
{
    for(int ch=0;ch<b.getNumChannels();++ch)
        for(int i=0;i<b.getNumSamples();++i)
            b.setSample(ch,i,float(.2*std::sin(2*juce::MathConstants<double>::pi*110*(offset+i)/rate)));
}
}
int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    juce::ScopedNoDenormals noDenormals;
    juce::MidiBuffer midi;
    PyramidFuzzAudioProcessor state;
    set(state,"input",7.1f);set(state,"sustain",8.3f);set(state,"tone",.78f);set(state,"high",.91f);
    set(state,"volume",-17);set(state,"voicing",0);set(state,"clippingMode",2);set(state,"oversampling",2);
    juce::MemoryBlock saved;state.getStateInformation(saved);
    PyramidFuzzAudioProcessor restored;restored.setStateInformation(saved.getData(),int(saved.getSize()));
    for(auto* parameter:state.getParameters())
    {
        auto* ranged=dynamic_cast<juce::RangedAudioParameter*>(parameter);
        check(std::abs(ranged->getValue()-restored.getValueTreeState().getParameter(ranged->paramID)->getValue())<1e-6f,"state round trip changes a parameter");
    }
    const auto original=restored.getValueTreeState().copyState().toXmlString();
    restored.setStateInformation(nullptr,0); const char bad[]="invalid";restored.setStateInformation(bad,sizeof(bad));
    for(int kind=0;kind<6;++kind)
    {
        auto tree=state.getValueTreeState().copyState();
        if(kind==0) tree=juce::ValueTree("OTHER");
        if(kind==1) tree.getChild(0).setProperty("value","nan",nullptr);
        if(kind==2) tree.getChild(0).setProperty("value",999999,nullptr);
        if(kind==3) tree.getChild(0).setProperty("value","unparseable",nullptr);
        if(kind==4) tree.appendChild(tree.getChild(0).createCopy(),nullptr);
        if(kind==5) tree.removeChild(0,nullptr);
        juce::MemoryBlock binary;juce::AudioProcessor::copyXmlToBinary(*tree.createXml(),binary);
        restored.setStateInformation(binary.getData(),int(binary.getSize()));
        check(restored.getValueTreeState().copyState().toXmlString()==original,"invalid state must leave current settings intact");
    }
    for(int in:{0,1,2,4}) for(int out:{0,1,2,4})
        check(state.isBusesLayoutSupported(layout(in,out))==((in==1&&(out==1||out==2))||(in==2&&out==2)),"unexpected supported bus layout");

    size_t callbackAllocations=0;double worstStep=0,peak=0;int fixedLatency=-1;
    for(double rate:{44100.0,48000.0,96000.0})
        for(auto route:{std::pair<int,int>{1,1},{1,2},{2,2}})
        {
            PyramidFuzzAudioProcessor processor;
            check(processor.setBusesLayout(layout(route.first,route.second)),"supported layout failed");
            processor.prepareToPlay(rate,64);
            const int latency=processor.getLatencySamples();
            if(fixedLatency<0) fixedLatency=latency;
            check(latency==fixedLatency,"prepared latency differs by sample rate");
            juce::AudioBuffer<float> storage(route.second,2048);
            float previous=0;int offset=0;
            for(int block=0;block<100;++block)
            {
                const int sizes[]={0,1,17,64,129,2048};const int size=sizes[block%6];
                juce::AudioBuffer<float> view(storage.getArrayOfWritePointers(),route.second,size);
                signal(view,offset,rate);
                if(route.first==1 && route.second==2) view.clear(1,0,size); // stale output must be replaced by input
                set(processor,"oversampling",float((block/3)%3));
                set(processor,"clippingMode",float((block/7)%3));
                set(processor,"tone",float((block/11)%2));set(processor,"high",float((block/13)%2));
                allocations=0;trackAllocations=true;
                processor.processBlock(view,midi);
                trackAllocations=false;callbackAllocations+=allocations;
                check(processor.getLatencySamples()==latency,"quality automation changes host latency");
                bool finite=true,stereo=true;
                for(int i=0;i<size;++i)
                {
                    const auto x=view.getSample(0,i);finite &= std::isfinite(x);
                    worstStep=std::max(worstStep,double(std::abs(x-previous)));previous=x;peak=std::max(peak,double(std::abs(x)));
                    if(route.second==2) stereo &= std::abs(x-view.getSample(1,i))<1e-6f;
                }
                check(finite,"non-finite processor output");check(stereo,"routing/stereo mismatch");
                offset+=size;
            }
            processor.releaseResources();processor.prepareToPlay(rate,32);
            check(processor.getLatencySamples()==latency,"reprepare latency changes");
        }
    check(callbackAllocations==0,"processing/quality changes allocated C++ heap storage");
    check(peak<1 && worstStep<.4,"quality/control/variable-block safety bounds exceeded");
    // Check that chunking changes no signal samples for fixed settings.
    PyramidFuzzAudioProcessor a,b;a.prepareToPlay(48000,32);b.prepareToPlay(48000,512);
    juce::AudioBuffer<float> x(2,2048),y(2,2048);signal(x,0,48000);y.makeCopyOf(x);
    a.processBlock(x,midi);b.processBlock(y,midi);double error=0;
    for(int ch=0;ch<2;++ch)for(int i=0;i<2048;++i)error=std::max(error,double(std::abs(x.getSample(ch,i)-y.getSample(ch,i))));
    check(error<1e-6,"chunk size changes fixed-control audio");
    x.setSample(0,0,std::numeric_limits<float>::quiet_NaN());a.processBlock(x,midi);
    for(int i=0;i<2048;++i)check(std::isfinite(x.getSample(0,i)),"non-finite input poisoned processing state");
    // A cancelled fade must keep the old path, and later requests must reach
    // the latest requested quality after the complete fade/reset sequence.
    pyramidfuzz::dsp::FuzzEngine transitions;
    pyramidfuzz::dsp::FuzzParameters request;
    transitions.updateParameters(request);transitions.prepare(48000,64,1);
    juce::AudioBuffer<float> transitionBuffer(1,64);transitionBuffer.clear();
    request.oversamplingMode=2;transitions.updateParameters(request);transitions.processBlock(transitionBuffer);
    request.oversamplingMode=1;transitions.updateParameters(request);transitions.processBlock(transitionBuffer);
    check(transitions.getActiveOversamplingMode()==1,"reversed request must cancel the pending quality change");
    for(int quality:{0,2,1,2,0})
    {
        request.oversamplingMode=quality;
        for(int block=0;block<40;++block)
        {
            transitions.updateParameters(request);transitionBuffer.clear();transitions.processBlock(transitionBuffer);
        }
        check(transitions.getActiveOversamplingMode()==quality,"quality transition did not reach requested path");
    }
    // DC group delay of each linear oversampler + padding matches one fixed value.
    using namespace pyramidfuzz::dsp;
    for(int mode=0;mode<3;++mode)
    {
        OversamplingEngine os;os.prepare(1,128);os.setMode(mode);
        juce::AudioBuffer<float> impulse(1,128);double sum=0,moment=0;
        for(int offset=0;offset<4096;offset+=128)
        {
            impulse.clear();if(offset==0)impulse.setSample(0,0,1);
            os.processBlock(impulse,[](auto&,int){});
            for(int i=0;i<128;++i){sum+=impulse.getSample(0,i);moment+=double(offset+i)*impulse.getSample(0,i);}
        }
        check(std::abs(moment/sum-os.getLatencySamples())<.02,"oversampler impulse delay disagrees with fixed report");
        std::cout<<"quality="<<(2<<mode)<<" native_latency="<<os.getNativeLatencySamples(mode)<<" compensated_delay="<<moment/sum<<'\n';
    }
    std::cout<<"callback_cpp_allocations="<<callbackAllocations<<" peak="<<peak<<" max_step="<<worstStep<<" chunk_error="<<error<<'\n';
    return failures==0?0:1;
}
