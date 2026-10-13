#!/usr/bin/env python3
# Finds memory displacements (d(rX) operands of loads/stores, >= 0x20 and
# not stack-relative) that the original uses in a function but ours never
# does, and vice versa: catches fields read or written at the wrong offset
# (a missing pad, a wrong header field) that callseq/abicheck miss.
# Usage: python3 tools/offcheck.py <unit> ...   |   --all
import re, sys
sys.path.insert(0, 'tools')
import subprocess

MINOFF = 4
MEM = re.compile(r'^(l|st)(bz|hz|ha|wz|fs|fd|wu|bu|hu|fsu|fdu|mw|wzu|bzu)?x?\b')


def offs(lines):
    out = set()
    for l in lines:
        m = re.match(r'\s*(\w+)\.?\s+(.*)', l)
        if not m:
            continue
        op, rest = m.group(1), m.group(2)
        if not re.match(r'^(lbz|lhz|lha|lwz|lfs|lfd|stb|sth|stw|stfs|stfd|psq_l|psq_st)u?$', op):
            continue
        d = re.search(r'(-?0x[0-9a-fA-F]+|-?\d+)\((r\d+)\)', rest)
        if not d or d.group(2) == 'r1' or '@' in rest:
            continue
        v = int(d.group(1), 16) if 'x' in d.group(1) else int(d.group(1))
        if v >= MINOFF:
            out.add(v)
    return out


def orig(unit):
    fns, cur = {}, None
    for l in open(f'build/GMFE69/asm/{unit if "/" in unit else "game/" + unit}.s'):
        m = re.match(r'\.fn (\S+),', l)
        if m:
            cur = m.group(1); fns[cur] = []; continue
        if l.startswith('.endfn'):
            cur = None; continue
        if cur:
            fns[cur].append(re.sub(r'/\*.*?\*/', '', l))
    return {k: offs(v) for k, v in fns.items()}


def ours(unit):
    obj = f'build/GMFE69/src/{unit if "/" in unit else "game/" + unit}.o'
    txt = subprocess.run(['build/binutils/powerpc-eabi-objdump', '-d', obj],
                         capture_output=True, text=True).stdout
    fns, cur = {}, None
    for l in txt.splitlines():
        m = re.match(r'[0-9a-f]+ <(\S+)>:', l)
        if m:
            cur = m.group(1); fns[cur] = []; continue
        p = l.split('\t')
        if cur and len(p) >= 3:
            fns[cur].append(p[2] + ' ' + (p[3] if len(p) > 3 else ''))
    return {k: offs(v) for k, v in fns.items()}


def check(unit):
    o, u = orig(unit), ours(unit)
    for fn in o:
        if fn not in u:
            continue
        a, b = o[fn] - u[fn], u[fn] - o[fn]
        if a or b:
            print(f'{unit} {fn}: only original {sorted(hex(x) for x in a)} only ours {sorted(hex(x) for x in b)}')


def units():
    for m in re.finditer(r'Object\(Equivalent, "game/([^"]+)\.(?:cpp|c)"',
                         open('configure.py').read()):
        yield m.group(1)


args = sys.argv[1:]
for unit in (list(units()) if args == ['--all'] else args):
    try:
        check(unit)
    except FileNotFoundError:
        pass
