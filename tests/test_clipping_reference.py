import math
from pathlib import Path
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from clipping_reference import pair, Reference

class ClippingReferenceTests(unittest.TestCase):
    def test_passive_monotone_junctions(self):
        for name,counts in (('1N914-like',(1,1)),('1N4001-like',(1,1)),('1N34A-like',(1,2))):
            previous=-math.inf
            for i in range(-100,101):
                v=i*.01; current=pair(v,name,counts)
                self.assertGreater(current,previous)
                self.assertGreaterEqual(v*current,0)
                previous=current
        self.assertAlmostEqual(pair(.4,'1N914-like'),-pair(-.4,'1N914-like'))
        self.assertGreater(pair(.3,'1N34A-like',(1,2)),20*abs(pair(-.3,'1N34A-like',(1,2))))
        self.assertGreater(pair(.3,'1N34A-like'),10*pair(.3,'1N4001-like'))

    def test_off_retains_amplifier_and_has_no_diode_current(self):
        ref=Reference(192000,True)
        for _ in range(2048): result=ref.process(.01,30000,0)
        self.assertGreater(result['output'],.01)
        self.assertEqual(result['diode_current'],0)
        self.assertLess(abs(result['residual']),1e-10)

if __name__=='__main__': unittest.main()
