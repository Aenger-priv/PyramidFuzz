#!/usr/bin/env python3
"""Generate standalone baseline figures. Requires matplotlib; never edits raw data."""
import argparse
import csv
import json
import math
from pathlib import Path
import os

# Keep renderer/font caches inside the supplied artifact directory.
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("bundle", type=Path)
args = parser.parse_args()
folder = args.bundle.resolve()
os.environ.setdefault("MPLCONFIGDIR", str(folder / "plot-cache"))
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt


def rows(name):
    with (folder / name).open(newline="") as stream:
        return list(csv.DictReader(stream))


def select(table, **values):
    return [r for r in table if all(abs(float(r[k]) - v) < 1e-6 for k, v in values.items())]


tone = rows("tone-reference.csv")
stages = rows("raw/stages.csv")
harmonics = rows("raw/harmonics.csv")
decay = rows("raw/decay.csv")
fig, axes = plt.subplots(2, 2, figsize=(13, 9), constrained_layout=True)
for high, color in ((0, "#2474ab"), (1, "#bf4824")):
    selected = select(tone, sample_rate=48000, tone=1, high=high)
    x = [float(r["frequency_hz"]) for r in selected]
    axes[0, 0].semilogx(x, [float(r["measured_db"]) for r in selected], color=color, label=f"Current, High={high}")
    axes[0, 0].semilogx(x, [float(r["schematic_same_pot_db"]) for r in selected], color=color, linestyle="--", label=f"Schematic, same pot position, High={high}")
axes[0, 0].set(title="Tone endpoint: topology comparison (48 kHz)", xlabel="Frequency (Hz)", ylabel="Gain (dB)", xlim=(20, 20000), ylim=(-80, 3))
axes[0, 0].legend(fontsize=8)
colors = ("#2474ab", "#bf4824", "#478148")
for mode, label, color in zip(range(3), ("Off", "Silicon", "Germanium"), colors):
    selected = [r for r in select(stages, sample_rate=48000, oversampling=4, mode=mode, voicing=1, sustain=.68) if r["stage"] == "output"]
    selected = list({r["input_peak_dbfs"]: r for r in selected}.values())
    selected.sort(key=lambda r: float(r["input_peak_dbfs"]))
    axes[0, 1].plot([float(r["input_peak_dbfs"]) for r in selected], [20 * math.log10(max(1e-12, float(r["rms"]))) for r in selected], "o-", color=color, label=label)
    chosen = min(selected, key=lambda r: abs(float(r["input_peak_dbfs"]) + 24))
    spectrum = [r for r in harmonics if r["case_id"] == chosen["case_id"]]
    axes[1, 0].plot([float(r["harmonic"]) for r in spectrum], [float(r["dbfs"]) for r in spectrum], "o-", markersize=3, color=color, label=label)
    tail = [r for r in decay if r["file"] == f"synthetic_plucks_4x_mode{mode}_voice1.wav"]
    axes[1, 1].plot([float(r["time_seconds"]) for r in tail], [20 * math.log10(max(1e-12, float(r["rms"]))) for r in tail], color=color, label=label)
axes[0, 1].set(title="Gain curve: Lo, 4x, Sustain 6.8", xlabel="Input sine peak (dBFS)", ylabel="Output RMS (dBFS)")
axes[1, 0].set(title="Harmonics: -24 dBFS peak sine, Lo, 4x", xlabel="Harmonic number (fundamental 111.33 Hz)", ylabel="Peak amplitude (dBFS)", ylim=(-140, 0))
axes[1, 1].axvline(4, color="gray", linestyle="--", label="Dry signal ends")
axes[1, 1].set(title="Synthetic plucks and tail (50 ms RMS windows)", xlabel="Time (s)", ylabel="Output RMS (dBFS)", ylim=(-140, 0))
for ax in axes.flat:
    ax.grid(True, which="both", alpha=.25)
    if ax != axes[0, 0]: ax.legend(fontsize=8)
manifest = json.loads((folder / "manifest.json").read_text())
identity = manifest.get("baseline_ref") or (manifest["head"][:7] + " captured working tree")
fig.suptitle(f"Pyramid Fuzz — {identity}\nSchematic comparison uses assumed 10 kΩ source / 220 kΩ load; no physical pedal calibration", fontsize=13)
fig.savefig(folder / "baseline-overview.png", dpi=160)
fig.savefig(folder / "baseline-overview.svg")
print(folder / "baseline-overview.png")
