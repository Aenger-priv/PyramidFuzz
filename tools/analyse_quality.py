#!/usr/bin/env python3
"""CPU and alias characterization, with a 32x model convergence reference.

Requires numpy/matplotlib from requirements-measurements.txt. Alias is estimated
as nonharmonic spectral energy for coherent sine excitation, not hardware error
or a perceptual rating. Timing is local wall-clock cost, not an OS deadline proof.
"""
import argparse
import csv
import hashlib
import json
import math
import os
from pathlib import Path
import struct

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--capture',type=Path,required=True)
parser.add_argument('--before',type=Path)
args=parser.parse_args();folder=args.capture.resolve()
os.environ.setdefault('MPLCONFIGDIR','/tmp/pyramid-quality-mpl')
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt


def rows(path):
    with path.open() as stream:return list(csv.DictReader(stream))


def audio(path):
    data=path.read_bytes();cursor=12;fmt=samples=None
    while cursor+8<=len(data):
        kind,size=struct.unpack_from('<4sI',data,cursor);chunk=data[cursor+8:cursor+8+size]
        if kind==b'fmt ':fmt=chunk
        if kind==b'data':samples=chunk
        cursor+=8+size+size%2
    tag,channels,rate,_,_,bits=struct.unpack_from('<HHIIHH',fmt)
    if tag==0xfffe:tag=struct.unpack_from('<H',fmt,24)[0]
    if tag!=3 or bits!=32 or channels!=1:raise ValueError('Expected mono float WAV')
    result=np.frombuffer(samples,dtype='<f4').astype(float)
    if not np.isfinite(result).all():raise ValueError('Non-finite audio')
    return result


def decimate_reference(x):
    # Independent 1025-tap Kaiser low-pass; analysis stops at 0.42 host Fs,
    # below the 0.46 Fs cutoff/transition, with edges reflected before FIR.
    taps=np.arange(1025)-512
    kernel=2*.115*np.sinc(2*.115*taps)*np.kaiser(1025,12)
    kernel/=kernel.sum()
    padded=np.pad(x,(512,512),mode='reflect')
    size=1<<(len(padded)+len(kernel)-2).bit_length()
    filtered=np.fft.irfft(np.fft.rfft(padded,size)*np.fft.rfft(kernel,size),size)
    return filtered[1024:1024+len(x):4]


def spectrum(x,host_rate,bin):
    n=len(x);phase=2*np.pi*np.arange(n)/(n-1)
    window=.35875-.48829*np.cos(phase)+.14128*np.cos(2*phase)-.01168*np.cos(3*phase)
    transformed=np.fft.rfft((x-x.mean())*window)
    energy=np.abs(transformed)**2
    frequencies=np.fft.rfftfreq(n,1/host_rate)
    band=(frequencies>=100)&(frequencies<=min(.42*host_rate,20000))
    nonharmonic=band.copy()
    for harmonic in range(1,int(n/2/bin)+1):
        center=harmonic*bin
        nonharmonic[max(0,center-5):center+6]=False
    rms=math.sqrt(2*float(energy[nonharmonic].sum())/(n*n*float(np.mean(window**2))))
    ratio=float(energy[nonharmonic].sum()/max(1e-30,energy[band].sum()))
    return 20*math.log10(max(1e-20,rms)),10*math.log10(max(1e-30,ratio))


cpu=rows(folder/'cpu.csv')
for row in cpu:
    for percentile in ('mean','p50','p95','p99','max'):
        row[percentile+'_deadline_fraction']=float(row[percentile+'_us'])/float(row['deadline_us'])
records=[]
for row in rows(folder/'spectra.csv'):
    signal=audio(folder/row['file'])
    if int(row['reference_multiplier'])==4:signal=decimate_reference(signal)
    absolute,relative=spectrum(signal,int(row['host_rate']),int(row['frequency_bin']))
    records.append({**row,'nonharmonic_rms_dbfs':absolute,'nonharmonic_db_relative':relative})
with (folder/'alias-analysis.csv').open('w',newline='') as stream:
    writer=csv.DictWriter(stream,fieldnames=list(records[0]));writer.writeheader();writer.writerows(records)
ref=[x for x in records if x['reference_multiplier']=='4']
measured=[x for x in records if x['reference_multiplier']=='1']
def key(row):return tuple(row[x] for x in ('sample_rate','quality','channels','block_size','automation'))
before={key(x):x for x in rows(args.before/'cpu.csv')} if args.before else {}
comparison=[]
for row in cpu:
    if key(row) in before:
        prior=before[key(row)]
        comparison.append({**{k:row[k] for k in ('sample_rate','quality','channels','block_size','automation')},
             'before_p95_fraction':float(prior['p95_us'])/float(prior['deadline_us']),
             'after_p95_fraction':row['p95_deadline_fraction']})
limits={'default_4x_48k_p95_fraction':.5,'all_modes_p95_fraction':.9,'4x_nonharmonic_db_relative':-60}
default=[r for r in cpu if r['sample_rate']=='48000' and r['quality']=='4']
summary={'cpu_cases':len(cpu),'spectral_cases':len(records),'reference_32x_cases':len(ref),
 'cpu_max_p95_fraction':max(r['p95_deadline_fraction'] for r in cpu),
 'cpu_max_p99_fraction':max(r['p99_deadline_fraction'] for r in cpu),
 'default_4x_48k_max_p95_fraction':max(r['p95_deadline_fraction'] for r in default),
 'limits':limits,
 'default_cpu_target_passed':max(r['p95_deadline_fraction'] for r in default)<.5,
 'all_modes_cpu_target_passed':max(r['p95_deadline_fraction'] for r in cpu)<.9,
 'alias_by_quality':{str(q):{'worst_relative_db':max(float(r['nonharmonic_db_relative']) for r in measured if int(r['quality'])==q),
     'worst_absolute_rms_dbfs':max(float(r['nonharmonic_rms_dbfs']) for r in measured if int(r['quality'])==q)} for q in (2,4,8)},
 '32x_reference_worst_relative_db':max(float(r['nonharmonic_db_relative']) for r in ref),
 'cpu_comparison':comparison,
 'method':'Coherent 0.4 peak sine; Hi/full Sustain/Tone/High; Off/Si/Ge; 44.1/48/96 kHz; frequencies 683/16384 Fs and 2423/16384 Fs. Blackman-Harris window; mask +/-5 bins around true in-band harmonics. Analysis 100 Hz to min(0.42 Fs,20 kHz). 32x reference uses 4x host Fs/8x internal and independent 1025-tap FIR decimation.',
 'limits_of_claim':'Nonharmonic energy includes numerical and transient residue; reference floor is reported. This does not measure all two-tone/guitar alias products, physical hardware, all OS loads or Intel performance. Wall-clock outliers are retained; no hard realtime guarantee.'}
summary['default_alias_target_passed']=summary['alias_by_quality']['4']['worst_relative_db']<=-60
if args.before:
    prior_cases={r['file']:r for r in rows(args.before/'spectra.csv')}
    audio_errors={}
    for r in rows(folder/'spectra.csv'):
        prior=prior_cases[r['file']]
        new=audio(folder/r['file']);old=audio(args.before/r['file'])
        shift=int(r['latency'])-int(prior['latency'])
        if shift>0:new,old=new[shift:],old[:-shift]
        if shift<0:new,old=new[:shift],old[-shift:]
        audio_errors[r['file']]=float(np.max(np.abs(new-old)))
    summary['latency_aligned_before_after_max_sample_error']=max(audio_errors.values())
    summary['latency_aligned_before_after_errors']=audio_errors
(folder/'quality-summary.json').write_text(json.dumps(summary,indent=2)+'\n')
fig,axes=plt.subplots(1,2,figsize=(12,4.5),constrained_layout=True)
for q,color in zip((2,4,8),('#4489a6','#b67c25','#795aa3')):
    chosen=[r for r in cpu if r['quality']==str(q) and r['channels']=='2' and r['block_size']=='128' and r['automation']=='1']
    axes[0].plot([int(r['sample_rate'])/1000 for r in chosen],[100*r['p95_deadline_fraction'] for r in chosen],marker='o',label=f'{q}x',color=color)
    chosen=[r for r in measured if r['quality']==str(q)]
    axes[1].scatter([int(r['host_rate'])/1000 for r in chosen],[float(r['nonharmonic_db_relative']) for r in chosen],label=f'{q}x',color=color,s=20,alpha=.7)
axes[0].axhline(100,color='r',ls='--',label='Buffer deadline')
axes[0].set(xlabel='Host rate (kHz)',ylabel='p95 buffer time (%)',title='Stereo, 128 frames, moving Tone/High')
axes[1].axhline(-60,color='gray',ls='--',label='4x engineering target')
axes[1].set(xlabel='Host rate (kHz)',ylabel='Nonharmonic energy relative to signal (dB)',title='Sine alias estimate; modes/frequencies shown')
for ax in axes:ax.grid(alpha=.25);ax.legend(fontsize=8)
fig.savefig(folder/'quality.png',dpi=180);fig.savefig(folder/'quality.pdf')
print(json.dumps({k:v for k,v in summary.items() if k!='cpu_comparison'},indent=2))
