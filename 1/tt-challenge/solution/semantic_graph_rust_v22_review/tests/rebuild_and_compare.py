#!/usr/bin/env python3
"""Requires installed Rust toolchain. Runs solver and verifies exact edge parity.
Usage: python3 tests/rebuild_and_compare.py --base /path/to/tt-challenge
"""
import argparse, json, pathlib, shutil, subprocess, sys, tempfile
p=argparse.ArgumentParser()
p.add_argument('--base',required=True,type=pathlib.Path)
a=p.parse_args()
root=pathlib.Path(__file__).resolve().parent.parent
cargo=shutil.which('cargo')
if not cargo:
 print('BLOCKED: cargo is not installed. Rust build and regeneration parity unverified.',file=sys.stderr)
 sys.exit(2)
subprocess.run([cargo,'test','--release','--locked'],cwd=root,check=True)
with tempfile.TemporaryDirectory() as td:
 generated=pathlib.Path(td)/'regenerated.json'
 subprocess.run([cargo,'run','--release','--locked','--','--base',str(a.base.resolve()),'--out',str(generated)],cwd=root,check=True)
 subprocess.run([sys.executable,str(root/'tests/check_submission.py'),'--base',str(a.base.resolve()),'--submission',str(root/'submission.json'),'--generated',str(generated)],cwd=root,check=True)
 print('PASS Rust binary builds; S0–S7 regenerated edge sets reproduce submission.json')
