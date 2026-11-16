#!/usr/bin/env python3
"""Small, independent complex AC circuit solver. No production DSP imports.

The netlist follows physical connections. This is a reference for the passive
network only; source/load impedances remain explicit modeling assumptions.
"""
import cmath
import json
import math
from pathlib import Path

NETLIST = Path(__file__).parent / "reference" / "tone-network.json"


def solve(matrix, rhs):
    """Complex Gaussian elimination with pivoting; inputs are not modified."""
    a = [list(row) + [value] for row, value in zip(matrix, rhs)]
    for col in range(len(a)):
        pivot = max(range(col, len(a)), key=lambda row: abs(a[row][col]))
        a[col], a[pivot] = a[pivot], a[col]
        if abs(a[col][col]) < 1e-20:
            raise ValueError("Singular AC network")
        divisor = a[col][col]
        a[col] = [value / divisor for value in a[col]]
        for row in range(col + 1, len(a)):
            multiplier = a[row][col]
            a[row] = [x - multiplier * y for x, y in zip(a[row], a[col])]
    result = [0j] * len(a)
    for row in reversed(range(len(a))):
        result[row] = a[row][-1] - sum(a[row][col] * result[col] for col in range(row + 1, len(a)))
    return result


def ac_response(netlist, frequency, *, values=None, sample_rate=None):
    """Continuous AC, or trapezoidal-equivalent AC if sample_rate is supplied."""
    components = dict(netlist["values"])
    components.update(values or {})
    known = netlist["known_voltages"]
    nodes = sorted({node for _, first, second, _ in netlist["branches"]
                    for node in (first, second)} - known.keys())
    indexes = {node: i for i, node in enumerate(nodes)}
    matrix = [[0j] * len(nodes) for _ in nodes]
    rhs = [0j] * len(nodes)
    omega = (2 * sample_rate * math.tan(math.pi * frequency / sample_rate)
             if sample_rate is not None else 2 * math.pi * frequency)
    for kind, first, second, value_name in netlist["branches"]:
        value = components[value_name]
        if value <= 0:
            raise ValueError("Component values must be positive")
        if kind not in ("R", "C"):
            raise ValueError(f"Unsupported component: {kind}")
        admittance = 1 / value if kind == "R" else 1j * omega * value
        for node, other in ((first, second), (second, first)):
            if node in known:
                continue
            i = indexes[node]
            matrix[i][i] += admittance
            if other in known:
                rhs[i] += admittance * known[other]
            else:
                matrix[i][indexes[other]] -= admittance
    return solve(matrix, rhs)[indexes[netlist["output"]]]


def controls(tone, high, *, legacy_position=False):
    """Target Tone is dark->bright. Legacy position preserves existing pot order.

    These are UI-to-resistance definitions, not corrections to the plugin.
    Endpoint floors avoid singular ideal shorts, matching current finite stops.
    """
    if not (0 <= tone <= 1 and 0 <= high <= 1):
        raise ValueError("Controls must be in [0, 1]")
    upper_fraction = tone if legacy_position else 1 - tone
    return {"Rbright": max(30.0, upper_fraction * 250000),
            "Rdark": max(30.0, (1 - upper_fraction) * 250000),
            "R8": max(20.0, (1 - high) * 25000)}


def response(frequency, tone, high, *, sample_rate=None, legacy_position=False, overrides=None):
    netlist = json.loads(NETLIST.read_text())
    values = controls(tone, high, legacy_position=legacy_position)
    values.update(overrides or {})
    return ac_response(netlist, frequency, values=values, sample_rate=sample_rate)


def db(value):
    return 20 * math.log10(max(1e-12, abs(value)))


def phase(value):
    return cmath.phase(value)
