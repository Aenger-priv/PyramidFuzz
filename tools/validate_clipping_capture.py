#!/usr/bin/env python3
"""Check runtime feedback traces against an independent bounded bisection solve."""
import argparse
import csv
import json
import math
from pathlib import Path
import subprocess
import tempfile
from clipping_reference import Reference, bias


def validate(folder):
    with (folder/'clipping-feedback.csv').open() as stream: rows=list(csv.DictReader(stream))
    errors={key:0.0 for key in ('base','output','diode_voltage','diode_current')}
    references={}; counts={}; failures=[]; residual=0; iterations=0
    for row in rows:
        x={k:float(v) for k,v in row.items()}
        if not all(math.isfinite(v) for v in x.values()): raise ValueError('Non-finite trace')
        case=int(x['case_id']); counts[case]=counts.get(case,0)+1
        if case not in references: references[case]=Reference(x['sample_rate'],bool(x['selectable']))
        ref=references[case].process(x['source'],x['source_resistance'],x['mode'])
        for key in errors: errors[key]=max(errors[key],abs(x[key]-ref[key]))
        residual=max(residual,abs(x['residual'])); iterations=max(iterations,int(x['iterations']))
        if x['selectable'] and x['mode']==0 and abs(x['diode_current'])>1e-15:
            failures.append('Off conducts diode current')
    if set(counts)!=set(range(15)) or set(counts.values())!={2048}: failures.append('Incomplete feedback trace matrix')
    if max(errors[k] for k in ('base','output','diode_voltage'))>2e-7 or errors['diode_current']>1e-10:
        failures.append('Feedback solution differs from independent reference')
    if residual>1e-9 or iterations>=48: failures.append('Feedback solve did not converge')
    return {'passed':not failures,'failures':failures,'cases':len(counts),'samples':len(rows),
            'max_absolute_errors':errors,'max_voltage_residual':residual,'max_iterations':iterations,'bias':bias(),
            'limits':'Numerical agreement for declared reduced model, not a physical-pedal or measured-diode match.'}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    source=parser.add_mutually_exclusive_group(required=True)
    source.add_argument('--executable',type=Path); source.add_argument('--capture',type=Path)
    parser.add_argument('--report',type=Path); args=parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='pyramid-clipping-') as temp:
        folder=args.capture
        if args.executable:
            folder=Path(temp)/'capture'
            subprocess.run([str(args.executable.resolve()),'--output',str(folder),'--clipping-only'],check=True)
        result=validate(folder)
    text=json.dumps(result,indent=2)+'\n'
    if args.report: args.report.write_text(text)
    print(text); raise SystemExit(0 if result['passed'] else 1)

if __name__=='__main__': main()
