#!/usr/bin/env python3
"""Plot the nominal input/Sustain model and its complete-engine gain behavior."""
import argparse
import csv
import os
import math
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--baseline", type=Path, required=True)
parser.add_argument("--revised", type=Path, required=True)
args = parser.parse_args()
old, new = args.baseline.resolve(), args.revised.resolve()
os.environ.setdefault("MPLCONFIGDIR", str(new / "plot-cache"))
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from input_reference import input_response, sustain_response, coupled_response


def rows(path):
    with path.open(newline="") as stream: return list(csv.DictReader(stream))


def select(data, **values):
    return [r for r in data if all(abs(float(r[k]) - v) < 1e-6 for k,v in values.items())]


data = rows(new / "raw/input-response.csv")
fig, axes = plt.subplots(2, 2, figsize=(12, 8.5), constrained_layout=True)
for voice, label, color in ((0,"Hi","#bf4824"),(1,"Lo","#2474ab")):
    pre = select([r for r in data if r["block"] == "preamp"],sample_rate=48000,oversampling=4,voicing=voice)
    f = [float(r["frequency_hz"]) for r in pre]
    axes[0,0].semilogx(f,[float(r["magnitude_db"]) for r in pre],"o-",color=color,label=label)
    axes[0,0].semilogx(f,[20*math.log10(abs(input_response(x,voice,192000))) for x in f],":",color=color)
    s_values = [i/100 for i in range(101)]
    axes[0,1].plot([10*s for s in s_values],[20*math.log10(abs(sustain_response(1000,s,voice,192000))) for s in s_values],color=color,label=label)
for s,color in ((0,"#444444"),(.25,"#2474ab"),(.68,"#478148"),(1,"#bf4824")):
    sustain = select([r for r in data if r["block"] == "sustain"],sample_rate=48000,oversampling=4,voicing=0,sustain=s)
    f = [float(r["frequency_hz"]) for r in sustain]
    axes[1,0].semilogx(f,[float(r["magnitude_db"]) for r in sustain],"o-",color=color,label=f"Sustain={10*s:g}")
    axes[1,0].semilogx(f,[20*math.log10(abs(sustain_response(x,s,0,192000))) for x in f],":",color=color)
for folder,label,style in ((old,"Step 2","--"),(new,"Step 3","-")):
    stages = rows(folder / "raw/stages.csv")
    for voice,name,color in ((0,"Hi","#bf4824"),(1,"Lo","#2474ab")):
        out = select([r for r in stages if r["stage"] == "output"],sample_rate=48000,oversampling=4,mode=1,voicing=voice,sustain=.68)
        out = list({r["input_peak_dbfs"]:r for r in out}.values()); out.sort(key=lambda r:float(r["input_peak_dbfs"]))
        axes[1,1].plot([float(r["input_peak_dbfs"]) for r in out],[20*math.log10(float(r["rms"])) for r in out],style,color=color,label=f"{label}, {name}")
axes[0,0].set(title="Fixed Q4 preamp and input loading",xlabel="Frequency (Hz)",ylabel="Source-to-preamp gain (dB)")
axes[0,1].set(title="Loaded Sustain travel at 1 kHz",xlabel="Sustain control",ylabel="Preamp-to-Q3 attenuation (dB)")
axes[1,0].set(title="C4/C5 coupling changes with Sustain (Hi)",xlabel="Frequency (Hz)",ylabel="Preamp-to-Q3 gain (dB)")
axes[1,1].set(title="Complete engine: Silicon, Sustain 6.8, 4x",xlabel="Input sine peak (dBFS)",ylabel="Output RMS (dBFS)")
for ax in axes.flat:
    ax.grid(True,which="both",alpha=.25); ax.legend(fontsize=8)
fig.suptitle("Pyramid Fuzz — input and Sustain revision\nDotted curves: independent nominal AC reference. Device assumptions are not measured pedal calibration.",fontsize=12)
fig.savefig(new / "input-sustain-revision.png",dpi=160)
fig.savefig(new / "input-sustain-revision.svg")
print(new / "input-sustain-revision.png")
