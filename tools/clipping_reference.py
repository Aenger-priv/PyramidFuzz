#!/usr/bin/env python3
"""Independent nominal feedback reference: DC KCL, TPT capacitors, bisection.

Uses no production DSP. Manufacturer maxima do not identify a unique low-current
curve; Is/N/R are explicit representative assumptions in the JSON netlist.
"""
import json
import math
from pathlib import Path
from tone_reference import solve

P = json.loads((Path(__file__).parent / 'reference/clipping-network.json').read_text())


def bias():
    beta, re, rc, rb, rf = (P[k] for k in ('beta','emitter','collector','base_bias','feedback'))
    vb, vc, ib = solve([[1,0,-re*(beta+1)], [1/rb+1/rf,-1/rf,1],
                       [-1/rf,1/rc+1/rf,beta]], [P['vbe'],0,P['supply']/rc])
    gm = beta*ib/P['thermal_voltage']
    rpi = beta/gm
    return {'gb':1/rb+1/(rpi+(beta+1)*re), 'gain':gm*rc/(1+re*(gm+1/rpi)),
            'positive_headroom':vc-(vb-P['vbe'])-P['saturation_voltage'],
            'negative_headroom':P['supply']-vc}


def forward(v, name, count=1):
    p = P['junctions'][name]
    k, r, saturation = count*p['N']*P['thermal_voltage'], count*p['R'], p['Is']
    transition = k*math.log1p(k/(r*saturation))
    if v > transition:
        return k/r + (saturation+k/r)/k*(v-transition)
    return saturation*math.expm1(v/k)


def pair(v, name, counts=(1,1)):
    return forward(v,name,counts[0])-forward(-v,name,counts[1])


class Reference:
    def __init__(self, rate, selectable):
        self.rate, self.selectable = rate, selectable
        self.q = bias()
        if not selectable:
            self.q['positive_headroom'], self.q['negative_headroom'] = self.q['negative_headroom'], self.q['positive_headroom']
        self.vf = self.ifb = self.vc = self.ic = 0.0

    def process(self, source, resistance, mode):
        gc, gd = 2*self.rate*P['feedback_cap'], 2*self.rate*P['diode_cap']
        hf, hd = -gc*self.vf-self.ifb, -gd*self.vc-self.ic
        si = 1-abs(mode-1) if self.selectable else 1
        ge = max(0,mode-1) if self.selectable else 0
        name = '1N4001-like' if self.selectable else '1N914-like'
        def residual(v):
            diode = si*pair(v,name)+ge*pair(v,'1N34A-like',(1,2))
            cap = (diode-hd)/gd
            diff = v+cap
            feedback = diff/P['feedback']+gc*diff+hf+diode
            base = (source/resistance-feedback)/(1/resistance+self.q['gb'])
            raw = self.q['gain']*base-P['collector']*feedback
            headroom = self.q['positive_headroom'] if raw >= 0 else self.q['negative_headroom']
            output = headroom*math.tanh(raw/headroom)
            return base+output-diff, base, output, diode, cap, diff
        lo, hi = -256.0, 256.0
        for _ in range(55):
            middle = (lo+hi)/2
            if residual(middle)[0] > 0: lo = middle
            else: hi = middle
        v = (lo+hi)/2
        error, base, output, diode, cap, diff = residual(v)
        self.ifb, self.vf = gc*diff+hf, diff
        self.ic, self.vc = diode, cap
        return {'base':base,'output':output,'diode_voltage':v,'diode_current':diode,'residual':error}
