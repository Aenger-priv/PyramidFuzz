#!/usr/bin/env python3
"""Check production tone responses against the independent Step 1 circuit.

Run as a CTest integration test with --executable, or validate an existing tone
CSV with --csv. The original broken baseline is expected to fail this gate.
"""
import argparse
import csv
import json
import math
from pathlib import Path
import subprocess
import tempfile

from tone_reference import db, phase, response


def rows(path):
    with path.open(newline="") as stream:
        return list(csv.DictReader(stream))


def validate(path):
    measured = rows(path)
    expected = {(rate, t, h) for rate in (44100, 48000, 96000)
                for t in (0, .25, .5, .75, 1) for h in (0, .25, .5, .75, 1)}
    curves = {(int(float(r["sample_rate"])), float(r["tone"]), float(r["high"])) for r in measured}
    failures = []
    if curves != expected:
        failures.append("Missing or unexpected tone curves")
    worst_gain = worst_phase = tail_peak = 0.0
    worst_gain_row = worst_phase_row = None
    checked = 0
    for row in measured:
        rate, t, h, f, gain, angle, tail = (float(row[k]) for k in
            ("sample_rate", "tone", "high", "frequency_hz", "magnitude_db", "phase_rad", "impulse_tail_peak"))
        if not all(math.isfinite(x) for x in (rate, t, h, f, gain, angle, tail)):
            failures.append("Non-finite response")
            continue
        tail_peak = max(tail_peak, tail)
        if not 30 <= f <= 10000:
            continue
        target = response(f, t, h, sample_rate=rate)
        gain_error = abs(gain - db(target))
        phase_error = abs(math.degrees(math.remainder(angle - phase(target), 2 * math.pi)))
        if gain_error > worst_gain:
            worst_gain, worst_gain_row = gain_error, row
        if db(target) > -80 and phase_error > worst_phase:
            worst_phase, worst_phase_row = phase_error, row
        checked += 1
    if checked < 5000:
        failures.append("Too few response points in the acceptance band")
    if worst_gain > .1:
        failures.append("Magnitude error exceeds 0.1 dB")
    if worst_phase > 1:
        failures.append("Phase error exceeds 1 degree")
    if tail_peak > 1e-6:
        failures.append("Impulse tail exceeds 1e-6")
    dynamics = []
    dynamic_path = path.with_name("tone-dynamics.csv")
    if dynamic_path.exists():
        dynamics = rows(dynamic_path)
        if {int(float(r["sample_rate"])) for r in dynamics} != {44100, 48000, 96000}:
            failures.append("Incomplete dynamic tone checks")
        for row in dynamics:
            values = {key: float(value) for key, value in row.items()}
            if not all(math.isfinite(x) for x in values.values()):
                failures.append("Non-finite dynamic tone check")
            # Conservative tone-only guards for a <=0.1 input and 30 ms ramps.
            # These are transient safety limits, not a claim of inaudible automation.
            if values["peak"] > .15 or values["max_sample_step"] > .05:
                failures.append("Unexpected control-sweep transient")
            if values["stereo_error"] > 1e-6 or values["tail_peak"] > 1e-6:
                failures.append("Dynamic channel or decay check failed")
    return {"passed": not failures, "failures": failures, "curves": len(curves),
            "checked_points": checked, "max_magnitude_error_db": worst_gain,
            "max_phase_error_degrees": worst_phase, "max_impulse_tail_peak": tail_peak,
            "worst_magnitude_row": worst_gain_row, "worst_phase_row": worst_phase_row,
            "tone_only_control_sweeps": dynamics,
            "assumptions": "22 nF C8; 10 kOhm source; 220 kOhm load; intended dark-to-bright UI mapping; trapezoidal AC reference"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--csv", type=Path)
    source.add_argument("--executable", type=Path)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="pyramid-tone-validation-") as temporary:
        path = args.csv
        if args.executable:
            output = Path(temporary) / "capture"
            subprocess.run([str(args.executable.resolve()), "--output", str(output), "--tone-only"], check=True)
            path = output / "tone.csv"
        report = validate(path)
    text = json.dumps(report, indent=2) + "\n"
    if args.report:
        args.report.write_text(text)
    print(text)
    raise SystemExit(0 if report["passed"] else 1)


if __name__ == "__main__":
    main()
