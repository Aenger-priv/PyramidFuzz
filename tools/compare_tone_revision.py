#!/usr/bin/env python3
"""Show the tone correction and prepare raw/RMS-matched synthetic comparisons.

Only exported comparison WAVs receive gain compensation. Plugin parameters and
captured raw data are never changed. Requires the plotting environment.
"""
import argparse
import csv
import hashlib
import json
import math
import os
from pathlib import Path
import shutil
import struct

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--baseline", type=Path, required=True)
parser.add_argument("--revised", type=Path, required=True)
args = parser.parse_args()
old, new = args.baseline.resolve(), args.revised.resolve()
output = new / "comparison"
if output.exists():
    parser.error("Comparison folder already exists; refusing to overwrite")
output.mkdir()
os.environ.setdefault("MPLCONFIGDIR", str(output / "plot-cache"))
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np


def rows(path):
    with path.open(newline="") as stream:
        return list(csv.DictReader(stream))


def select(table, **values):
    return [r for r in table if all(abs(float(r[k]) - value) < 1e-6 for k, value in values.items())]


def read_float_wave(path):
    raw = path.read_bytes()
    cursor, fmt, samples = 12, None, None
    if raw[:4] != b"RIFF" or raw[8:12] != b"WAVE":
        raise ValueError("Expected RIFF/WAVE")
    while cursor + 8 <= len(raw):
        kind, count = struct.unpack_from("<4sI", raw, cursor)
        data = raw[cursor + 8:cursor + 8 + count]
        if kind == b"fmt ": fmt = data
        if kind == b"data": samples = data
        cursor += 8 + count + count % 2
    tag, channels, rate, _, _, bits = struct.unpack_from("<HHIIHH", fmt)
    if tag == 0xFFFE: tag = struct.unpack_from("<H", fmt, 24)[0]
    if tag != 3 or bits != 32 or samples is None:
        raise ValueError("Expected float32 capture")
    audio = np.frombuffer(samples, dtype="<f4").astype(np.float64).reshape(-1, channels)
    if not np.isfinite(audio).all(): raise ValueError("Non-finite capture")
    return rate, audio


def write_pcm24(path, rate, audio):
    if np.max(np.abs(audio)) >= 1:
        raise ValueError("Comparison would clip; refusing to limit or normalize silently")
    pcm = np.rint(audio.reshape(-1) * 8388608).astype(np.int32)
    packed = np.column_stack((pcm & 255, (pcm >> 8) & 255, (pcm >> 16) & 255)).astype(np.uint8).tobytes()
    channels = audio.shape[1]
    fmt = struct.pack("<HHIIHH", 1, channels, rate, rate * channels * 3, channels * 3, 24)
    chunks = b"fmt " + struct.pack("<I", len(fmt)) + fmt + b"data" + struct.pack("<I", len(packed)) + packed
    if len(packed) % 2: chunks += b"\0"
    path.write_bytes(b"RIFF" + struct.pack("<I", 4 + len(chunks)) + b"WAVE" + chunks)


sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
dry_name = "synthetic_plucks_dry.wav"
if sha(old / "raw" / dry_name) != sha(new / "raw" / dry_name):
    raise ValueError("Dry comparison inputs differ")
old_protocol = json.loads((old / "raw/protocol.json").read_text())
new_protocol = json.loads((new / "raw/protocol.json").read_text())
if old_protocol != new_protocol:
    raise ValueError("Comparison protocols/defaults differ")

comparisons = []
for mode, name in enumerate(("off", "silicon", "germanium")):
    file = f"synthetic_plucks_4x_mode{mode}_voice1.wav"
    old_rate, before = read_float_wave(old / "raw" / file)
    new_rate, after = read_float_wave(new / "raw" / file)
    if old_rate != new_rate or before.shape != after.shape:
        raise ValueError("Comparison rate/shape mismatch")
    before_rms = float(np.sqrt(np.mean(before[:4 * old_rate] ** 2)))
    after_rms = float(np.sqrt(np.mean(after[:4 * new_rate] ** 2)))
    if min(before_rms, after_rms) <= 0: raise ValueError("Silent comparison")
    target = min(before_rms, after_rms)
    gains = (target / before_rms, target / after_rms)
    for label, folder, audio, gain in (("original", old, before, gains[0]), ("corrected", new, after, gains[1])):
        shutil.copyfile(folder / "raw" / file, output / f"{name}-{label}-raw.wav")
        write_pcm24(output / f"{name}-{label}-rms-matched.wav", old_rate, audio * gain)
    comparisons.append({"mode": name, "original_source_sha256": sha(old / "raw" / file),
                        "corrected_source_sha256": sha(new / "raw" / file),
                        "original_rms_dbfs": 20 * math.log10(before_rms),
                        "corrected_rms_dbfs": 20 * math.log10(after_rms),
                        "original_listening_gain_db": 20 * math.log10(gains[0]),
                        "corrected_listening_gain_db": 20 * math.log10(gains[1])})
shutil.copyfile(new / "raw" / dry_name, output / dry_name)
metadata = {"stimulus": "synthetic plucks, not recorded guitar", "dry_sha256": sha(new / "raw" / dry_name),
            "parameters": new_protocol["defaults"], "sample_rate": 48000,
            "comparison": "same input and control values; tone direction intentionally corrected",
            "matching": "RMS over first four seconds; attenuate louder version only; no perceptual-loudness claim",
            "compensation_is_in_plugin": False, "modes": comparisons}
(output / "listening-settings.json").write_text(json.dumps(metadata, indent=2) + "\n")

old_tone, new_tone = rows(old / "raw/tone.csv"), rows(new / "tone-reference.csv")
fig, axes = plt.subplots(1, 2, figsize=(12, 4.8), constrained_layout=True)
for high, color in ((0, "#2474ab"), (1, "#bf4824")):
    before = select(old_tone, sample_rate=48000, tone=1, high=high)
    after = select(new_tone, sample_rate=48000, tone=1, high=high)
    axes[0].semilogx([float(r["frequency_hz"]) for r in before], [float(r["magnitude_db"]) for r in before], color=color, linestyle="--", label=f"Original, High={high}")
    axes[0].semilogx([float(r["frequency_hz"]) for r in after], [float(r["measured_db"]) for r in after], color=color, label=f"Corrected, High={high}")
for tone, color in zip((0, .25, .5, .75, 1), ("#444444", "#2474ab", "#478148", "#b47820", "#bf4824")):
    curve = select(new_tone, sample_rate=48000, tone=tone, high=.5)
    x = [float(r["frequency_hz"]) for r in curve]
    axes[1].semilogx(x, [float(r["measured_db"]) for r in curve], color=color, label=f"Tone={tone:g}")
    axes[1].semilogx(x, [float(r["schematic_intended_ui_db"]) for r in curve], color=color, linestyle=":")
axes[0].set(title="Same UI endpoint: the near-mute corner is removed", ylabel="Gain (dB)", ylim=(-80, 3))
axes[1].set(title="Corrected dark-to-bright sweep (High=0.5)\nDotted curves: independent circuit reference", ylabel="Gain (dB)", ylim=(-45, 3))
for ax in axes:
    ax.set(xlabel="Frequency (Hz)", xlim=(20, 20000))
    ax.grid(True, which="both", alpha=.25)
    ax.legend(fontsize=8)
fig.suptitle("Pyramid Fuzz — tone correction at 48 kHz\n22 nF C8; assumed 10 kΩ source / 220 kΩ load; no gain compensation in DSP", fontsize=12)
fig.savefig(output / "tone-correction.png", dpi=160)
fig.savefig(output / "tone-correction.svg")
print(output)
