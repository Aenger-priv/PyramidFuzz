"""Analytic checks for the independent reference, not implementation snapshots."""
import math
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from tone_reference import ac_response, controls, response


class ReferenceTests(unittest.TestCase):
    def test_resistive_divider(self):
        circuit = {"known_voltages": {"0": 0, "in": 1}, "output": "out",
                   "values": {"a": 1000, "b": 2000},
                   "branches": [["R", "in", "out", "a"], ["R", "out", "0", "b"]]}
        self.assertAlmostEqual(abs(ac_response(circuit, 1000)), 2 / 3, places=12)

    def test_rc_filters_against_closed_form(self):
        for high_pass in (False, True):
            circuit = {"known_voltages": {"0": 0, "in": 1}, "output": "out",
                       "values": {"r": 10000, "c": 22e-9},
                       "branches": ([["C", "in", "out", "c"], ["R", "out", "0", "r"]]
                                    if high_pass else [["R", "in", "out", "r"], ["C", "out", "0", "c"]])}
            for rate in (None, 44100, 96000):
                for frequency in (20, 100, 1000, 10000):
                    w = 2 * math.pi * frequency if rate is None else 2 * rate * math.tan(math.pi * frequency / rate)
                    s_rc = 1j * w * 10000 * 22e-9
                    expected = (s_rc if high_pass else 1) / (1 + s_rc)
                    self.assertLess(abs(ac_response(circuit, frequency, sample_rate=rate) - expected), 1e-12)

    def test_passive_network_and_direction(self):
        for tone in (0, 0.25, 0.5, 0.75, 1):
            for high in (0, 0.5, 1):
                self.assertLess(abs(response(1e-4, tone, high)), 1e-4)
                for frequency in (20, 100, 1000, 5000, 20000):
                    self.assertLessEqual(abs(response(frequency, tone, high)), 1.0000001)
        # At the dark endpoint, reducing R8 must raise the high-frequency corner.
        self.assertGreater(abs(response(5000, 0, 1)), abs(response(5000, 0, 0)))
        # Bright and dark are spectral tilts, not merely different output gains.
        tilt = lambda tone: abs(response(5000, tone, 0)) / abs(response(100, tone, 0))
        self.assertGreater(tilt(1), tilt(0))
        self.assertEqual(controls(0.25, 0.5), controls(0.75, 0.5, legacy_position=True))


if __name__ == "__main__":
    unittest.main()
