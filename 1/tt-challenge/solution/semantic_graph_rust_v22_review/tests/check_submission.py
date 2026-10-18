#!/usr/bin/env python3
"""Offline verifier; not used to create any prediction edges.
Usage: python3 tests/check_submission.py --base ../tt-challenge
Optional: --generated regenerated.json to compare Rust output with submission.
"""
import argparse, gzip, json, pathlib, sys
p=argparse.ArgumentParser()
p.add_argument('--base',required=True,type=pathlib.Path)
p.add_argument('--submission',type=pathlib.Path,default=pathlib.Path('submission.json'))
p.add_argument('--generated',type=pathlib.Path)
a=p.parse_args()
with gzip.open(a.base/'skeleton/trainticket.json.gz','rt',encoding='utf-8') as f: nodes=json.load(f)['nodes']
methods={k for k,v in nodes.items() if v.get('kind')=='method'}
tables={k for k,v in nodes.items() if v.get('kind')=='data_resource'}
doc=json.loads(a.submission.read_text())
assert doc['project']=='trainticket'
assert set(doc['scenarios'])=={f'S{i}' for i in range(8)}
sets={}
for sid,block in doc['scenarios'].items():
    seen=set()
    for e in block['edges']:
        src,rel,dst=e['source'],e['relation'],e['target']
        assert rel in ('executes','calls','remote','data')
        if rel=='executes':assert src==f'scenario:{sid}' and dst in methods
        elif rel=='data':assert src in methods and dst in tables
        else:assert src in methods and dst in methods
        item=(src,rel,dst)
        assert item not in seen, ('duplicate',sid,item)
        seen.add(item)
    sets[sid]=seen
    print(sid,len(seen),'edges, valid')

gold=json.loads((a.base/'gold/S0.json').read_text())['scenarios']['S0']
expected={(f'scenario:S0','executes',x) for x in gold['methods']}
for rel in ['calls','remote','data']:
    expected.update((src,rel,dst) for src,dst in gold[rel])
assert sets['S0']==expected,'S0 mismatch'
print('S0 exact public gold: PASS')
if a.generated:
    candidate=json.loads(a.generated.read_text())
    for sid in sets:
        observed={(x['source'],x['relation'],x['target']) for x in candidate['scenarios'][sid]['edges']}
        if sets[sid]!=observed:
            print(f'RUST PARITY FAILED {sid}: missing {len(sets[sid]-observed)}, extra {len(observed-sets[sid])}')
            sys.exit(1)
    print('Rust regeneration parity: PASS')
print('Format and identifiers: PASS')
