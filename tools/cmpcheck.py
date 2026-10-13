#!/usr/bin/env python3
# Finds compare immediates (cmpwi/cmplwi/cmpw against a small constant) that
# the original uses in a function but our object never does: catches dropped
# switch cases and wrong enum values (e.g. CreatePlayer lost "case 3", so
# hard mode reset the player's health). Usage:
#   python3 tools/cmpcheck.py <unit> ...      (game/<unit> basename, or dir/unit)
#   python3 tools/cmpcheck.py --all           (every Equivalent game unit)
# Switches compiled as jump tables do not show up here.
import re, subprocess, sys


def upath(unit):
    return unit if '/' in unit else f'game/{unit}'


def parse_int(t):
    return int(t, 16) if t.lower().startswith(('0x', '-0x')) else int(t)


def imms(lines):
    out = set()
    hi = set()  # registers holding an addis/subis result (32-bit compares)
    for l in lines:
        m = re.match(r'\s*(\w+)\.?\s+(.*)', l)
        if not m:
            continue
        op, args = m.group(1), [a.strip() for a in m.group(2).split(',')]
        if op in ('addis', 'subis'):
            hi.add(args[0])
            continue
        if op not in ('cmpwi', 'cmplwi'):
            if args and args[0] in hi:
                hi.discard(args[0])
            continue
        reg, imm = (args[0], args[1]) if len(args) == 2 else (args[1], args[2])
        if reg in hi:
            continue
        try:
            out.add((op, parse_int(imm)))
        except ValueError:
            pass
    return out


def orig(unit):
    fns = {}
    cur = None
    for l in open(f'build/GMFE69/asm/{upath(unit)}.s'):
        m = re.match(r'\.fn (\S+),', l)
        if m:
            cur = m.group(1)
            fns[cur] = []
            continue
        if l.startswith('.endfn'):
            cur = None
            continue
        if cur:
            fns[cur].append(re.sub(r'/\*.*?\*/', '', l))
    return {k: imms(v) for k, v in fns.items()}


def ours(unit, src):
    obj = f'build/GMFE69/src/{upath(src)}.o'
    txt = subprocess.run(['build/binutils/powerpc-eabi-objdump', '-dr', obj],
                         capture_output=True, text=True).stdout
    syms = subprocess.run(['build/binutils/powerpc-eabi-objdump', '-t', obj],
                          capture_output=True, text=True).stdout
    local = {l.split()[-1] for l in syms.splitlines() if ' l ' in l and ' F ' in l}
    fns = {}
    calls = {}
    cur = None
    for l in txt.splitlines():
        m = re.match(r'[0-9a-f]+ <(\S+)>:', l)
        if m:
            cur = m.group(1)
            fns[cur] = []
            calls[cur] = set()
            continue
        if not cur:
            continue
        m = re.search(r'R_PPC_REL24\s+(\S+)', l)
        if m:
            calls[cur].add(m.group(1))
            continue
        p = l.split('\t')
        if len(p) >= 3:
            fns[cur].append(p[2] + ' ' + (p[3] if len(p) > 3 else ''))
    res = {k: imms(v) for k, v in fns.items()}

    # Fold in our out-of-line local helpers (inlined in the original).
    def merged(fn, seen):
        out = set(res.get(fn, ()))
        for c in calls.get(fn, ()):
            if c in local and c not in seen:
                seen.add(c)
                out |= merged(c, seen)
        return out
    return {k: merged(k, {k}) for k in res}


def units():
    for m in re.finditer(r'Object\(Equivalent, "game/([^"]+)\.(?:cpp|c)"',
                         open('configure.py').read()):
        yield m.group(1)


def check(unit):
    o = orig(unit)
    u = ours(unit, unit)
    for fn, c in o.items():
        if fn not in u:
            continue
        ourvals = {v for _, v in u[fn]}
        miss = sorted({v for _, v in c if v not in ourvals})
        if miss:
            print(f'{unit} {fn}: missing compares against {miss}')


args = sys.argv[1:]
for unit in (list(units()) if args == ['--all'] else args):
    try:
        check(unit)
    except FileNotFoundError as e:
        print(f'{unit}: {e}')
