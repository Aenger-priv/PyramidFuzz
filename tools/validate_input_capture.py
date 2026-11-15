#!/usr/bin/env python3
"""Validate production input/Sustain responses against independent nominal KCL.

The tighter circuit agreement is a numerical/model-consistency check, not a
claim that the chosen transistor beta or guitar/interface voltage scale is measured.
"""
import argparse
import cmath
import csv
import json
import math
from pathlib import Path
import subprocess
import tempfile

from input_reference import input_response, sustain_response, coupled_response, bias, input_resistance, output_resistance
from tone_reference import db


def rows(path):
    with path.open(newline="") as stream: return list(csv.DictReader(stream))


def validate(folder):
    measured = rows(folder / "input-response.csv")
    expected = set()
    for rate in (44100, 48000, 96000):
        for factor in (2, 4, 8):
            frequencies = (20,50,100,440,1000,3000,5000,10000) if factor == 4 else (20,1000,10000)
            settings = (0,.25,.68,1) if factor == 4 else (0,1)
            for voice in (0, 1):
                for f in frequencies:
                    expected.add(("preamp", rate, factor, voice, 0, f))
                    for s in settings: expected.add(("sustain", rate, factor, voice, s, f))
    keyed = {}
    failures = []
    worst_gain = worst_phase = coupled_gain = coupled_phase = 0.0
    for row in measured:
        kind = row["block"]
        rate, factor, voice, s, f, gain, angle = (float(row[k]) for k in
            ("sample_rate", "oversampling", "voicing", "sustain", "frequency_hz", "magnitude_db", "phase_rad"))
        if not all(math.isfinite(x) for x in (rate,factor,voice,s,f,gain,angle)):
            failures.append("Non-finite input response"); continue
        key = (kind, int(rate), int(factor), int(voice), round(s, 3), int(f))
        if key in keyed: failures.append("Duplicate input measurement")
        value = cmath.rect(10 ** (gain / 20), angle)
        keyed[key] = value
        reference = (input_response(f, int(voice), rate * factor) if kind == "preamp"
                     else sustain_response(f, s, int(voice), rate * factor))
        worst_gain = max(worst_gain, abs(db(value / reference)))
        worst_phase = max(worst_phase, abs(math.degrees(cmath.phase(value / reference))))
    if set(keyed) != expected: failures.append("Missing/unexpected input response cases")
    for key, value in keyed.items():
        kind, rate, factor, voice, s, f = key
        if kind != "sustain": continue
        before = keyed[("preamp", rate, factor, voice, 0, f)]
        target = coupled_response(f, s, voice, rate * factor)
        ratio = before * value / target
        coupled_gain = max(coupled_gain, abs(db(ratio)))
        coupled_phase = max(coupled_phase, abs(math.degrees(cmath.phase(ratio))))
    if worst_gain > .1 or worst_phase > 1:
        failures.append("Block response exceeds 0.1 dB / 1 degree")
    if coupled_gain > .1 or coupled_phase > 1:
        failures.append("Combined response exceeds 0.1 dB / 1 degree versus coupled nominal circuit")
    # Circuit-level behavior must remain useful at the pot ends and between them.
    for voice in (0, 1):
        pre = abs(keyed[("preamp",48000,4,voice,0,100)])
        if voice == 0: hi_gain = pre
        else:
            if hi_gain < 5 * pre: failures.append("Hi/Lo drive distinction lost")
        values = [abs(keyed[("sustain",48000,4,voice,s,1000)]) for s in (0,.25,.68,1)]
        if not all(a < b for a,b in zip(values,values[1:])) or values[-1] < 25 * values[0] or values[0] <= 0:
            failures.append("Loaded Sustain divider endpoint/travel check failed")
    dynamic = rows(folder / "input-dynamics.csv")
    if len(dynamic) != 5: failures.append("Missing hot-input dynamic cases")
    for row in dynamic:
        v = {k:float(x) for k,x in row.items()}
        if not all(math.isfinite(x) for x in v.values()): failures.append("Non-finite dynamics")
        # Hot input (.4 peak and up to +18 dB trim), changing Sustain/voicing.
        # These are output safety bounds, not proof of inaudible automation.
        if v["peak"] > .9 or v["max_sample_step"] > .15 or v["stereo_error"] > 1e-6:
            failures.append("Hot input or control-switching safety check failed")
        if v["tail_peak"] > 1e-4: failures.append("Hot-input tail did not settle after C2 discharge window")
    return {"passed": not failures, "failures": failures, "response_cases": len(measured),
            "max_block_magnitude_error_db": worst_gain, "max_block_phase_error_degrees": worst_phase,
            "max_coupled_magnitude_error_db": coupled_gain, "max_coupled_phase_error_degrees": coupled_phase,
            "q4_bias": bias("q4"), "q3_input_resistance_ohms": input_resistance("q3"),
            "q4_midband_output_ohms": [output_resistance(v) for v in (0,1)],
            "hot_input_dynamics": dynamic,
            "limits": "Input/Sustain isolated AC uses nominal Q3 load; engine uses its nonlinear base port. Eight-second dynamics allow physical C2 discharge; early_tail_peak reports the original three-second window. No physical-pedal calibration."}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--executable", type=Path)
    source.add_argument("--capture", type=Path)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="pyramid-input-validation-") as temporary:
        folder = args.capture
        if args.executable:
            folder = Path(temporary) / "capture"
            subprocess.run([str(args.executable.resolve()), "--output", str(folder), "--input-only"], check=True)
        report = validate(folder)
    text = json.dumps(report, indent=2) + "\n"
    if args.report: args.report.write_text(text)
    print(text)
    raise SystemExit(0 if report["passed"] else 1)


if __name__ == "__main__": main()
