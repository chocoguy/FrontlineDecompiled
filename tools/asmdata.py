#!/usr/bin/env python3
# Prints C initializers for data objects of an original asm unit, as rows of
# signed/unsigned bytes, halfwords or words grouped `--group` per element.
# usage: python3 tools/asmdata.py movie/getvlc PMBtab0 [--group 2] [--signed]
#        [--width 1|2|4] [--per-line 8]
import argparse, re

ap = argparse.ArgumentParser()
ap.add_argument('unit'); ap.add_argument('names', nargs='+')
ap.add_argument('--group', type=int, default=1)
ap.add_argument('--width', type=int, default=1)
ap.add_argument('--signed', action='store_true')
ap.add_argument('--per-line', type=int, default=8)
a = ap.parse_args()
objs, cur = {}, None
for l in open(f'build/GMFE69/asm/{a.unit}.s'):
    m = re.match(r'\.obj (\S+),', l)
    if m:
        cur = m.group(1).strip('"'); objs[cur] = bytearray(); continue
    if l.startswith('.endobj'):
        cur = None; continue
    if cur is None:
        continue
    m = re.match(r'\s*\.(4byte|2byte|byte)\s+(.*)', l)
    if m:
        n = {'4byte': 4, '2byte': 2, 'byte': 1}[m.group(1)]
        for v in m.group(2).split(','):
            try:
                objs[cur] += int(v.strip(), 0).to_bytes(n, 'big')
            except ValueError:  # a symbol reference
                objs[cur] += bytes(n)
    m = re.match(r'\s*\.skip\s+(\S+)', l)
    if m:
        objs[cur] += bytes(int(m.group(1), 0))
for name in a.names:
    b = objs[name]
    vals = [int.from_bytes(b[i:i + a.width], 'big', signed=a.signed) for i in range(0, len(b), a.width)]
    els = [vals[i:i + a.group] for i in range(0, len(vals), a.group)]
    fmt = (lambda e: '{' + ', '.join(map(str, e)) + '}') if a.group > 1 else (lambda e: str(e[0]))
    print(f'// {name}: {len(els)} elements')
    for i in range(0, len(els), a.per_line):
        print('    ' + ', '.join(fmt(e) for e in els[i:i + a.per_line]) + ',')
