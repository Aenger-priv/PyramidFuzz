import math
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from input_reference import PARAMETERS as P, ac, bias, input_resistance, output_resistance, input_response, sustain_response, coupled_response


class InputReferenceTests(unittest.TestCase):
    def test_controlled_current_source(self):
        branches = [("R", "out", "0", 10000), ("G", "out", "0", "in", "0", .001)]
        self.assertAlmostEqual(ac(branches, {"in": 1, "0": 0}, "out").real, -10)

    def test_bias_satisfies_circuit_kcl(self):
        for stage in ("q4", "q3"):
            q = bias(stage)
            self.assertGreater(q["collector"], q["emitter"] + .2)
            self.assertLess(q["collector"], P["supply"])
            self.assertAlmostEqual((q["collector"] - q["base"]) / P["feedback"], q["base_current"] + q["base"] / P["base_bias"], places=12)
            self.assertAlmostEqual((P["supply"] - q["collector"]) / P["collector"], q["collector_current"] + (q["collector"] - q["base"]) / P["feedback"], places=12)

    def test_resistance_and_dc_endpoints(self):
        self.assertLess(input_resistance("q4"), P["base_bias"])
        for voice in (0, 1):
            self.assertTrue(0 < output_resistance(voice) < P["collector"])
            for sustain in (0, .25, .68, 1):
                self.assertLess(abs(sustain_response(1e-6, sustain, voice)), 1e-8)
                self.assertLess(abs(sustain_response(1000, sustain, voice)), 1)
            self.assertGreater(abs(sustain_response(1000, 1, voice)), 25 * abs(sustain_response(1000, 0, voice)))
            self.assertGreater(abs(sustain_response(1000, 0, voice)), 0)
        self.assertGreater(abs(input_response(100, 0)), 5 * abs(input_response(100, 1)))

    def test_block_decoupling_is_exact_in_midband_limit(self):
        # With C1/C4/C5 ideal shorts and C10 absent, the Thevenin split must
        # reproduce the coupled network at every pot setting, independently of DSP.
        p = dict(P, input_cap=1e6, c4=1e6, c5=1e6, feedback_cap=1e-30)
        for voice in (0, 1):
            for sustain in (0, .25, .68, 1):
                # Huge capacitor conductances can make nodal elimination ill-conditioned;
                # a moderate frequency and finite but large caps approximate this limit.
                p.update(input_cap=1, c4=1, c5=1)
                divided = input_response(100, voice, p=p) * sustain_response(100, sustain, voice, p=p)
                coupled = coupled_response(100, sustain, voice, p=p)
                self.assertLess(abs(divided / coupled - 1), 1e-5)


if __name__ == "__main__": unittest.main()
