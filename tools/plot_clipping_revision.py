#!/usr/bin/env python3
"""Plot declared diode curves and measured mode differences; export audition modes.

Requires the optional measurement plotting environment. Only comparison WAVs
are attenuated for RMS matching; raw renders and production gains are untouched.
"""
import argparse
import csv
import json
import math
import os
from pathlib import Path
import struct
import wave
from clipping_reference import forward

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--capture',type=Path,required=True)
args=parser.parse_args(); root=args.capture.resolve(); output=root/'clipping-review'
output.mkdir(exist_ok=False)
os.environ.setdefault('MPLCONFIGDIR',str(output/'plot-cache'))
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np


def rows(name):
    with (root/'raw'/name).open() as stream: return list(csv.DictReader(stream))


def choose(table,**values):
    return [r for r in table if all(r[k]==v if isinstance(v,str) else abs(float(r[k])-v)<1e-6 for k,v in values.items())]


fig,axes=plt.subplots(2,2,figsize=(12,8),constrained_layout=True)
voltage=np.linspace(0,.9,500)
for name,count,label in [('1N914-like',1,'Stage A silicon'),('1N4001-like',1,'Stage B silicon'),
                          ('1N34A-like',1,'Ge, one junction'),('1N34A-like',2,'Ge, two junctions')]:
    axes[0,0].semilogy(voltage,[max(1e-12,forward(v,name,count)) for v in voltage],label=label)
axes[0,0].set(xlabel='Forward branch voltage (V)',ylabel='Current (A)',ylim=(1e-8,1e-3),
              title='Declared nominal junction curves, 25.85 mV thermal voltage')
trace=rows('clipping-feedback.csv')
labels=['Off','Silicon','Germanium']; colors=['#666666','#287aa4','#bd7925']
for mode,label,color in zip(range(3),labels,colors):
    data=choose(trace,sample_rate=192000,selectable=1,mode=mode)
    # Exclude the morph case, which only touches exact endpoints at its ends.
    data=[r for r in data if int(r['case_id'])==6+mode]
    axes[0,1].plot([float(r['sample'])/192 for r in data],[float(r['output']) for r in data],label=label,color=color)
axes[0,1].set(xlabel='Time (ms)',ylabel='Stage B collector signal (model V)',title='Same source, capacitor states start at zero')
stages=rows('stages.csv')
for mode,label,color in zip(range(3),labels,colors):
    data=choose(stages,stage='output',sample_rate=48000,oversampling=4,voicing=1,sustain=.68,mode=mode)
    data={float(r['input_peak_dbfs']):r for r in data}
    levels=sorted(data)
    axes[1,0].plot(levels,[20*math.log10(float(data[x]['rms'])) for x in levels],marker='o',color=color,label=label)
    case=data[-24]['case_id']
    harmonics=choose(rows('harmonics.csv'),case_id=case)
    fundamental=float(harmonics[0]['dbfs'])
    axes[1,1].plot([int(r['harmonic']) for r in harmonics],[float(r['dbfs'])-fundamental for r in harmonics],marker='.',label=label,color=color)
axes[1,0].set(xlabel='Input sine peak (dBFS)',ylabel='Output RMS (dBFS)',title='Full engine, Lo, Sustain 6.8, default Tone/High/Volume')
axes[1,1].set(xlabel='Harmonic number',ylabel='Relative to fundamental (dB)',ylim=(-100,5),title='Full engine harmonics at −24 dBFS input peak')
for ax in axes.flat:
    ax.grid(alpha=.25); ax.legend(fontsize=8)
fig.suptitle('Step 4: nominal clipping/recovery model — no hardware-pedal calibration',fontsize=13)
fig.savefig(output/'clipping-model.png',dpi=180)
fig.savefig(output/'clipping-model.pdf')


def read_float(path):
    raw=path.read_bytes(); cursor=12; fmt=data=None
    while cursor+8<=len(raw):
        kind,count=struct.unpack_from('<4sI',raw,cursor); payload=raw[cursor+8:cursor+8+count]
        if kind==b'fmt ': fmt=payload
        if kind==b'data': data=payload
        cursor+=8+count+count%2
    tag,channels,rate,_,_,bits=struct.unpack_from('<HHIIHH',fmt)
    if tag==0xfffe: tag=struct.unpack_from('<H',fmt,24)[0]
    if tag!=3 or bits!=32: raise ValueError('Expected float32 source')
    return rate,np.frombuffer(data,dtype='<f4').astype(float).reshape(-1,channels)


metadata=[]
for voice in range(2):
    audio=[read_float(root/'raw'/f'synthetic_plucks_4x_mode{mode}_voice{voice}.wav') for mode in range(3)]
    rms=[float(np.sqrt(np.mean(a[:4*rate]**2))) for rate,a in audio]
    target=min(rms)
    for mode,((rate,a),level) in enumerate(zip(audio,rms)):
        gain=target/level; matched=a*gain
        if not np.isfinite(matched).all() or np.max(np.abs(matched))>=1: raise ValueError('Unsafe listening export')
        integers=np.rint(matched.reshape(-1)*8388608).astype(np.int32)
        packed=np.column_stack((integers&255,(integers>>8)&255,(integers>>16)&255)).astype(np.uint8).tobytes()
        name=f'{labels[mode].lower()}-{"hi" if voice==0 else "lo"}-modes-rms-matched.wav'
        with wave.open(str(output/name),'wb') as stream:
            stream.setnchannels(a.shape[1]); stream.setsampwidth(3); stream.setframerate(rate); stream.writeframes(packed)
        metadata.append({'file':name,'mode':labels[mode],'voice':'Hi' if voice==0 else 'Lo',
                         'raw_rms_dbfs':20*math.log10(level),'listening_gain_db':20*math.log10(gain)})
(output/'listening-settings.json').write_text(json.dumps({'stimulus':'Synthetic plucks, not real guitar DI',
    'matching':'RMS over first four seconds; attenuate louder modes only; no production compensation',
    'files':metadata},indent=2)+'\n')
print(output)
