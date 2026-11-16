#!/usr/bin/env python3
"""Capture a new immutable measurement bundle and independent passive reference.

No Python dependencies for CSV/reference/validation. Plotting is a separate tool.
Never overwrites an existing raw capture or manifest, including failed runs.
"""
import argparse
import csv
from datetime import datetime, timezone
import hashlib
import json
import math
from pathlib import Path
import platform
import struct
import subprocess
import sys
import tarfile

from tone_reference import db, phase, response

ROOT = Path(__file__).resolve().parents[1]


def sha256(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def command(*args):
    return subprocess.check_output(args, cwd=ROOT, text=True, stderr=subprocess.STDOUT).strip()


def source_files():
    files = [ROOT / name for name in ("CMakeLists.txt", ".gitmodules")]
    for folder in ("src", "tools", "tests"):
        files.extend(p for p in (ROOT / folder).rglob("*")
                     if p.is_file() and "__pycache__" not in p.parts and p.suffix != ".pyc")
    return sorted(files)


def source_hashes():
    return {str(p.relative_to(ROOT)): sha256(p) for p in source_files()}


def read_csv(path):
    with path.open(newline="") as stream:
        return list(csv.DictReader(stream))


def write_csv(path, fields, rows):
    with path.open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


def validate_float_wav(path):
    """Check floating WAV format, finite samples, and raw peak without normalizing."""
    data = path.read_bytes()
    if data[:4] != b"RIFF" or data[8:12] != b"WAVE":
        raise ValueError(f"Invalid WAV: {path}")
    cursor = 12
    fmt = samples = None
    while cursor + 8 <= len(data):
        kind, size = struct.unpack_from("<4sI", data, cursor)
        chunk = data[cursor + 8:cursor + 8 + size]
        if kind == b"fmt ":
            fmt = chunk
        elif kind == b"data":
            samples = chunk
        cursor += 8 + size + size % 2
    if fmt is None or samples is None:
        raise ValueError(f"Missing WAV chunks: {path}")
    tag, channels, rate, _, _, bits = struct.unpack_from("<HHIIHH", fmt)
    if tag == 0xFFFE:
        tag = struct.unpack_from("<H", fmt, 24)[0]
    if tag != 3 or bits != 32 or len(samples) % (4 * channels):
        raise ValueError(f"Expected float32 WAV: {path}")
    peak = 0.0
    for (sample,) in struct.iter_unpack("<f", samples):
        if not math.isfinite(sample):
            raise ValueError(f"Non-finite WAV sample: {path}")
        peak = max(peak, abs(sample))
    return {"channels": channels, "sample_rate": rate,
            "frames": len(samples) // (4 * channels), "peak": peak}


def analyse(folder):
    raw = folder / "raw"
    tone = read_csv(raw / "tone.csv")
    stages = read_csv(raw / "stages.csv")
    harmonics = read_csv(raw / "harmonics.csv")
    renders = read_csv(raw / "renders.csv")
    decay = read_csv(raw / "decay.csv")
    for table in (tone, stages, harmonics, renders, decay):
        if not table:
            raise ValueError("Empty measurement table")
        for row in table:
            for key, value in row.items():
                if key not in ("stage", "file", "source") and not math.isfinite(float(value)):
                    raise ValueError(f"Non-finite CSV value: {key}={value}")
    grid = {(int(float(r["sample_rate"])), float(r["tone"]), float(r["high"])) for r in tone}
    expected = {(rate, t, h) for rate in (44100, 48000, 96000)
                for t in (0, .25, .5, .75, 1) for h in (0, .25, .5, .75, 1)}
    if grid != expected:
        raise ValueError("Incomplete tone grid")
    if max(float(r["impulse_tail_peak"]) for r in tone) > 1e-6:
        raise ValueError("Tone impulse did not settle; increase capture length")
    reference = []
    for row in tone:
        rate, t, h, f = (float(row[key]) for key in ("sample_rate", "tone", "high", "frequency_hz"))
        same_position = response(f, t, h, sample_rate=rate, legacy_position=True)
        intended = response(f, t, h, sample_rate=rate)
        analog = response(f, t, h)
        reference.append({**{k: row[k] for k in ("sample_rate", "tone", "high", "frequency_hz")},
                          "measured_db": row["magnitude_db"],
                          "schematic_same_pot_db": db(same_position),
                          "schematic_intended_ui_db": db(intended),
                          "schematic_analog_ui_db": db(analog),
                          "schematic_intended_phase": phase(intended)})
    write_csv(folder / "tone-reference.csv", list(reference[0]), reference)
    sensitivity = []
    for rs in (5000, 10000, 20000):
        for rl in (100000, 220000, 470000):
            for f in (100, 1000, 5000):
                sensitivity.append({"source_ohms": rs, "load_ohms": rl, "frequency_hz": f,
                                    "tone": .5, "high": .5,
                                    "magnitude_db": db(response(f, .5, .5, overrides={"Rsource": rs, "Rload": rl}))})
    write_csv(folder / "loading-sensitivity.csv", list(sensitivity[0]), sensitivity)
    ids = sorted({int(r["case_id"]) for r in stages})
    stage_names = {r["stage"] for r in stages}
    expected_stages = {"dry", "input_voicing", "pre_gain", "sustain", "clip_a", "clip_b", "downsampled", "tone", "recovery", "output"}
    if (len(ids) != 117 or stage_names != expected_stages or len(stages) != 117 * len(expected_stages)
            or len(harmonics) != 117 * 32):
        raise ValueError("Incomplete gain/stage/harmonic matrix")
    tap_error = max(float(r["tap_max_error"]) for r in stages + renders)
    if tap_error > 1e-6:
        raise ValueError("Tap adapter does not match production engine")
    wavs = {p.name: validate_float_wav(p) for p in raw.glob("*.wav")}
    for row in renders:
        measured = wavs[row["file"]]
        if abs(measured["peak"] - float(row["peak"])) > 1e-6:
            raise ValueError("WAV peak does not match render statistics")
    def anchor(high):
        candidates = [r for r in tone if float(r["sample_rate"]) == 48000 and float(r["tone"]) == 1 and float(r["high"]) == high]
        return min(candidates, key=lambda r: abs(float(r["frequency_hz"]) - 1000))
    low, high = anchor(0), anchor(1)
    mode_levels = {}
    for row in stages:
        if (row["stage"] == "output" and float(row["sample_rate"]) == 48000 and int(row["oversampling"]) == 4
                and row["voicing"] == "1" and abs(float(row["sustain"]) - .68) < 1e-6 and float(row["input_peak_dbfs"]) == -24):
            mode_levels[row["mode"]] = {"rms_dbfs": db(float(row["rms"])), "peak_dbfs": db(float(row["peak"])), "dc": float(row["dc"])}
    return {"tone_curves": len(grid), "tone_rows": len(tone), "gain_cases": len(ids),
            "stage_rows": len(stages), "harmonic_rows": len(harmonics), "render_count": len(renders),
            "wav_count": len(wavs), "tap_max_error": tap_error,
            "tone_corner": {"sample_rate": 48000, "tone": 1, "frequency_hz": float(low["frequency_hz"]),
                            "high_0_db": float(low["magnitude_db"]), "high_1_db": float(high["magnitude_db"]),
                            "change_db": float(high["magnitude_db"]) - float(low["magnitude_db"])},
            "mode_levels_at_minus24_peak_dbfs_lo_4x": mode_levels,
            "max_tone_impulse_tail_peak": max(float(r["impulse_tail_peak"]) for r in tone),
            "wavs": wavs,
            "limits": "Capture completeness/safety only. Use separate circuit-response checks for model consistency. No alias, CPU, hardware-match, or listening approval is implied."}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--di", type=Path, action="append", default=[])
    parser.add_argument("--baseline-ref", help="Require unchanged production src/assets/JUCE pin relative to this commit")
    args = parser.parse_args()
    folder = args.output.resolve()
    folder.mkdir(parents=True, exist_ok=True)
    if (folder / "manifest.json").exists() or (folder / "raw").exists():
        parser.error("Capture exists; use a fresh output directory")
    executable = args.executable.resolve()
    if not executable.is_file():
        parser.error("Measurement executable does not exist")
    if command("git", "-C", "JUCE", "status", "--porcelain"):
        parser.error("JUCE working tree must be clean")
    if args.baseline_ref:
        changed = command("git", "diff", args.baseline_ref, "--", "src", "assets", "JUCE", ".gitmodules")
        extra = command("git", "ls-files", "--others", "--exclude-standard", "--", "src", "assets")
        if changed or extra:
            parser.error("Production code/assets/JUCE pin differ from requested baseline")
    hashes = source_hashes()
    manifest = {"schema": 1, "status": "capturing", "started_utc": datetime.now(timezone.utc).isoformat(),
                "head": command("git", "rev-parse", "HEAD"), "branch": command("git", "branch", "--show-current"),
                "baseline_ref": args.baseline_ref, "working_tree": command("git", "status", "--porcelain"),
                "juce_commit": command("git", "-C", "JUCE", "rev-parse", "HEAD"),
                "platform": platform.platform(), "machine": platform.machine(), "python": sys.version,
                "xcode": command("xcodebuild", "-version"), "clang": command("xcrun", "clang", "--version"),
                "cmake": command("cmake", "--version"), "sdk": command("xcrun", "--show-sdk-version"),
                "executable": str(executable), "executable_sha256": sha256(executable), "source_sha256": hashes,
                "command": [str(executable), "--output", str(folder / "raw")],
                "di_inputs": [{"path": str(p.resolve()), "sha256": sha256(p)} for p in args.di],
                "session_policy": "User chose revised sound for existing sessions; original build retained separately. No legacy DSP model planned.",
                "signal_scale": "Input: 1 digital unit = 1 nominal model volt; output: 3 model volts = 1 digital unit before Volume. This is an explicit convention, not measured interface/pedal calibration."}
    for p in args.di:
        manifest["command"].extend(["--di", str(p.resolve())])
    (folder / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    try:
        with tarfile.open(folder / "measurement-source.tar.gz", "w:gz") as archive:
            for path in source_files():
                archive.add(path, arcname=str(path.relative_to(ROOT)))
        with (folder / "measurement.log").open("w") as log:
            subprocess.run(manifest["command"], check=True, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT)
        summary = analyse(folder)
        manifest["protocol"] = json.loads((folder / "raw" / "protocol.json").read_text())
        if source_hashes() != hashes:
            raise ValueError("Measurement sources changed during capture")
        preservation = folder / "preserved" / "preservation.json"
        if preservation.exists():
            preserved = json.loads(preservation.read_text())
            for name, expected in preserved["files"].items():
                if sha256(preservation.parent / name) != expected:
                    raise ValueError("Preserved archive checksum mismatch")
            manifest["preservation"] = preserved
        (folder / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
        manifest["status"] = "complete"
        manifest["finished_utc"] = datetime.now(timezone.utc).isoformat()
        manifest["artifacts_sha256"] = {str(p.relative_to(folder)): sha256(p)
            for p in sorted(folder.rglob("*")) if p.is_file() and p.name != "manifest.json"}
    except Exception as error:
        manifest["status"] = "failed"
        manifest["error"] = str(error)
        raise
    finally:
        (folder / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(json.dumps({k: v for k, v in summary.items() if k != "wavs"}, indent=2))
    print(f"Saved {folder}")


if __name__ == "__main__":
    main()
