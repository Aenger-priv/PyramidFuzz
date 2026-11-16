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
int main(int argc,char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    juce::ScopedNoDenormals noDenormals;
    juce::MidiBuffer midi;
    if(argc==2)
    {
        const auto folder=juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]);
        if(folder.exists() || !folder.createDirectory()) return 2;
        for(bool hot:{false,true})
        {
            PyramidFuzzAudioProcessor preview;
            if(hot){set(preview,"input",18);set(preview,"sustain",10);set(preview,"tone",1);set(preview,"high",1);set(preview,"volume",12);set(preview,"clippingMode",0);}
            preview.prepareToPlay(48000,128);juce::AudioBuffer<float> buffer(2,128);
            for(int i=0;i<32;++i){signal(buffer,i*128,48000);preview.processBlock(buffer,midi);}
            std::unique_ptr<juce::AudioProcessorEditor> editor(preview.createEditor());
            for(float scale:{1.0f,2.0f})
            {
                auto image=editor->createComponentSnapshot(editor->getLocalBounds(),true,scale);
                auto file=folder.getChildFile(juce::String(hot?"editor-overload-":"editor-default-")+juce::String(int(scale))+"x.png");
                auto stream=file.createOutputStream();juce::PNGImageFormat png;
                if(!stream || !png.writeImageToStream(image,*stream)) return 3;
            }
        }
        return 0;
    }
    // Meter timing is sample-based, so callback size cannot change the release
    // rate or overload hold. Levels above full scale remain visible to the UI.
    for(double rate:{44100.0,48000.0,96000.0})for(int size:{32,128,512})
    {
        pyramidfuzz::dsp::PeakMeter meter;meter.prepare(rate);meter.push(1.4f,1);
        check(meter.level()>1.39f && meter.overload(),"meter must expose overload without clamping the audio level");
        int frames=int(rate*.4);
        for(int offset=0;offset<frames;offset+=size)meter.push(0,std::min(size,frames-offset));
        check(std::abs(meter.level()-1.4f*std::exp(-1.0f))<1e-4f,"meter release depends on block size");
        check(meter.overload(),"clip hold expired too soon");
        meter.push(0,int(rate)-frames-1);check(meter.overload(),"clip hold must last one second");
        meter.push(0,1);check(!meter.overload(),"clip hold did not expire");
    }
    PyramidFuzzAudioProcessor formatted;
    auto* toneParameter=formatted.getValueTreeState().getParameter("tone");
    check(toneParameter->getText(.5f,0)=="50.0" && toneParameter->getLabel()=="%","Tone must expose readable percent formatting");
    check(std::abs(toneParameter->convertFrom0to1(toneParameter->getValueForText("44.5%"))-.445f)<1e-6,"percent text must preserve the parameter's stored range");
    auto* inputParameter=formatted.getValueTreeState().getParameter("input");
    check(inputParameter->getText(inputParameter->convertTo0to1(8),0)=="+8.0" && inputParameter->getLabel()=="dB","trim must expose signed dB formatting");
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
    PyramidFuzzAudioProcessor independent;independent.prepareToPlay(48000,64);
    juce::AudioBuffer<float> rightOnly(2,2048);rightOnly.clear();
    for(int i=0;i<2048;++i)rightOnly.setSample(1,i,float(.2*std::sin(2*juce::MathConstants<double>::pi*1000*i/48000)));
    independent.processBlock(rightOnly,midi);
    check(rightOnly.getMagnitude(0,0,2048)<1e-8f && rightOnly.getMagnitude(1,0,2048)>.001f,"independent stereo input must not leak or duplicate channels");
    // Native host bypass must preserve latency and mono-to-stereo expansion.
    // It must also ignore Volume while keeping the processing state warm.
    for(auto route:{std::pair<int,int>{1,1},{1,2},{2,2}})
    {
        PyramidFuzzAudioProcessor bypass;
        bypass.setBusesLayout(layout(route.first,route.second));bypass.prepareToPlay(48000,32);
        juce::AudioBuffer<float> block(route.second,512);signal(block,0,48000);
        if(route.first==2)for(int i=0;i<512;++i)block.setSample(1,i,float(.13*std::sin(2*juce::MathConstants<double>::pi*337*i/48000)));
        juce::AudioBuffer<float> original;original.makeCopyOf(block);
        allocations=0;trackAllocations=true;bypass.processBlockBypassed(block,midi);trackAllocations=false;
        check(allocations==0,"host bypass allocated C++ heap storage");
        const int delay=bypass.getLatencySamples();double bypassError=0;
        for(int ch=0;ch<route.second;++ch)for(int i=0;i<512;++i)
            bypassError=std::max(bypassError,double(std::abs(block.getSample(ch,i)-(i<delay?0:original.getSample(ch,i-delay)))));
        check(bypassError<1e-7,"host bypass latency or routing is incorrect");
        double step=0;float previous=block.getSample(0,511);
        for(int part=1;part<30;++part)
        {
            signal(block,part*512,48000);
            if(part%6<3)bypass.processBlock(block,midi);else bypass.processBlockBypassed(block,midi);
            for(int i=0;i<512;++i){const auto v=block.getSample(0,i);step=std::max(step,double(std::abs(v-previous)));previous=v;}
        }
        check(std::isfinite(step)&&step<.1,"host bypass transition exceeds safety step bound");
    }
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
