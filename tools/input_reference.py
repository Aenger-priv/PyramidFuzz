#!/usr/bin/env python3
"""Independent DC KCL and complex AC reference for the input/Sustain blocks.

Includes a fully coupled small-signal reference to quantify, rather than hide,
the runtime model's midband source/load decoupling. No production DSP imports.
"""
import json
import math
from pathlib import Path

from tone_reference import solve

PARAMETERS = json.loads((Path(__file__).parent / "reference/input-network.json").read_text())


def bias(stage, parameters=None):
    p = dict(PARAMETERS); p.update(parameters or {})
    beta, re = p[stage + "_beta"], p[stage + "_emitter"]
    rc, rb, rf = p["collector"], p["base_bias"], p["feedback"]
    # Unknown Vb, Vc, Ib; independent KCL formulation rather than C++ formula.
    vb, vc, ib = solve([[1, 0, -re * (beta + 1)],
                        [1 / rb + 1 / rf, -1 / rf, 1],
                        [-1 / rf, 1 / rc + 1 / rf, beta]], [p["vbe"], 0, p["supply"] / rc])
    ve, ic = vb - p["vbe"], beta * ib
    gm, rpi = ic / p["thermal_voltage"], beta * p["thermal_voltage"] / ic
    return {"base": vb, "collector": vc, "emitter": ve, "base_current": ib,
            "collector_current": ic, "gb": 1 / (rpi + (beta + 1) * re),
            "gm": gm / (1 + re * (gm + 1 / rpi))}


def ac(branches, known, output, frequency=0, rate=None, injection=None):
    """R/C branches and voltage-controlled current sources (G)."""
    nodes = sorted({node for b in branches for node in b[1:-1]} - known.keys())
    index = {node: i for i, node in enumerate(nodes)}
    matrix = [[0j] * len(nodes) for _ in nodes]; rhs = [0j] * len(nodes)
    omega = 2 * math.pi * frequency if rate is None else 2 * rate * math.tan(math.pi * frequency / rate)
    def stamp(row, col, value):
        if row in known: return
        if col in known: rhs[index[row]] -= value * known[col]
        else: matrix[index[row]][index[col]] += value
    for branch in branches:
        if branch[0] in ("R", "C"):
            kind, a, b, value = branch
            y = 1 / value if kind == "R" else 1j * omega * value
            stamp(a, a, y); stamp(a, b, -y); stamp(b, a, -y); stamp(b, b, y)
        else:
            _, plus, minus, control_plus, control_minus, gm = branch
            stamp(plus, control_plus, gm); stamp(plus, control_minus, -gm)
            stamp(minus, control_plus, -gm); stamp(minus, control_minus, gm)
    for node, current in (injection or {}).items(): rhs[index[node]] += current
    return solve(matrix, rhs)[index[output]]


def transistor(stage, p):
    q = bias(stage, p)
    return [("R", "base", "0", p["base_bias"]), ("R", "base", "0", 1 / q["gb"]),
            ("R", "collector", "0", p["collector"]), ("R", "collector", "base", p["feedback"]),
            ("G", "collector", "0", "base", "0", q["gm"])]


def input_resistance(stage, p=None):
    p = p or PARAMETERS
    vc = ac(transistor(stage, p), {"base": 1, "0": 0}, "collector")
    q = bias(stage, p)
    return 1 / (1 / p["base_bias"] + q["gb"] + (1 - vc.real) / p["feedback"])


def output_resistance(voice, p=None):
    p = p or PARAMETERS
    branches = transistor("q4", p) + [("R", "base", "0", p["hi"] if voice == 0 else p["lo"])]
    return ac(branches, {"0": 0}, "collector", injection={"collector": 1}).real


def input_response(frequency, voice, rate=None, p=None):
    p = p or PARAMETERS
    branches = transistor("q4", p) + [
        ("R", "source", "coupled", p["hi"] if voice == 0 else p["lo"]),
        ("C", "coupled", "base", p["input_cap"]),
        ("C", "collector", "base", p["feedback_cap"])]
    return -ac(branches, {"source": 1, "0": 0}, "collector", frequency, rate)


def output_network(voice, p=None):
    p = p or PARAMETERS
    q = bias("q4", p)
    a = 1 / (p["hi"] if voice == 0 else p["lo"]) + 1 / p["base_bias"] + q["gb"]
    total = 1 / p["collector"] + a + q["gm"]
    constant = a / p["collector"] + total / p["feedback"]
    series = 1 / total
    parallel = output_resistance(voice, p) - series
    capacitance = p["feedback_cap"] * total / constant / parallel
    return series, parallel, capacitance


def sustain_response(frequency, sustain, voice, rate=None, p=None):
    p = p or PARAMETERS
    load = input_resistance("q3", p)
    series, parallel, cap = output_network(voice, p)
    branches = [("R", "source", "impedance", series),
                ("R", "impedance", "capacitor", parallel), ("C", "impedance", "capacitor", cap),
                ("C", "capacitor", "coupled", p["c4"]),
                ("R", "coupled", "wiper", max(1e-3, (1 - sustain) * p["pot"])),
                ("R", "wiper", "0", p["pot_bottom"] + sustain * p["pot"]),
                ("C", "wiper", "load", p["c5"]), ("R", "load", "base", p["r19"]),
                ("R", "base", "0", load)]
    return ac(branches, {"source": 1, "0": 0}, "base", frequency, rate)


def coupled_response(frequency, sustain, voice, rate=None, p=None):
    p = p or PARAMETERS
    top = "wiper" if sustain == 1 else "top"
    branches = transistor("q4", p) + [
        ("R", "source", "coupled", p["hi"] if voice == 0 else p["lo"]),
        ("C", "coupled", "base", p["input_cap"]), ("C", "collector", "base", p["feedback_cap"]),
        ("C", "collector", top, p["c4"]),
        ("R", "wiper", "0", p["pot_bottom"] + sustain * p["pot"]),
        ("C", "wiper", "load", p["c5"]), ("R", "load", "q3base", p["r19"]),
        ("R", "q3base", "0", input_resistance("q3", p))]
    if sustain != 1: branches.append(("R", top, "wiper", (1 - sustain) * p["pot"]))
    return -ac(branches, {"source": 1, "0": 0}, "q3base", frequency, rate)
