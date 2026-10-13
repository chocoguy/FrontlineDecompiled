#!/usr/bin/env python3
# Compares the initialized data objects (.data/.rodata/.sdata/.sdata2) of a
# unit's original asm with the same-named symbols in our object, byte for
# byte (relocated words are skipped). Compiler literals (@N) are ignored.
# usage: python3 tools/datacmp.py movie/getvlc [...]
import re, subprocess, sys

def orig(unit):
    objs, cur, sec = {}, None, ''
    for l in open(f'build/GMFE69/asm/{unit}.s'):
        if l.startswith('.section') or l.startswith('.data') or l.startswith('.rodata') or l.startswith('.text'):
            sec = l.split()[1] if l.startswith('.section') else l.split()[0]
        m = re.match(r'\.obj (\S+),', l)
        if m:
            cur = m.group(1).strip('"'); objs[cur] = [sec.strip(','), bytearray(), set()]; continue
        if l.startswith('.endobj'):
            cur = None; continue
        if cur is None:
            continue
        m = re.match(r'\s*\.(4byte|2byte|byte|float|double|string|skip)\s+(.*)', l)
        if not m:
            continue
        kind, arg = m.groups()
        b = objs[cur][1]
        if kind == 'skip':
            b += bytes(int(arg, 0))
        elif kind == 'string':
            b += bytes(arg.strip()[1:-1], 'ascii').decode('unicode_escape').encode('latin1') + b'\0'
        elif kind in ('float', 'double'):
            import struct
            b += struct.pack('>f' if kind == 'float' else '>d', float(arg))
        else:
            n = {'4byte': 4, '2byte': 2, 'byte': 1}[kind]
            for v in arg.split(','):
                v = v.strip()
                try:
                    b += (int(v, 0) & ((1 << (8 * n)) - 1)).to_bytes(n, 'big')
                except ValueError:
                    objs[cur][2].add(len(b)); b += bytes(n)
    return objs

def ours(unit):
    obj = f'build/GMFE69/src/{unit}.o'
    syms = {}
    out = subprocess.run(['build/binutils/powerpc-eabi-objdump', '-t', obj], capture_output=True, text=True).stdout
    for l in out.splitlines():
        p = l.split()
        if len(p) >= 5 and p[-2].startswith('0') and p[-3].startswith('.'):
            syms[p[-1]] = (p[-3], int(p[0], 16), int(p[-2], 16))
    secs = {}
    for s in set(v[0] for v in syms.values()):
        tmp = 'build/scratch/datacmp.bin'
        subprocess.run(['build/binutils/powerpc-eabi-objcopy', '-O', 'binary', '-j', s, obj, tmp], capture_output=True)
        secs[s] = open(tmp, 'rb').read()
    return syms, secs

for unit in sys.argv[1:]:
    o = orig(unit); syms, secs = ours(unit)
    bad = 0
    for name, (sec, b, relocs) in o.items():
        if name.startswith('@') or name.startswith('gap_') or sec in ('.bss', '.sbss', 'extab', 'extabindex', '.ctors', '.dtors') or not b:
            continue
        if name not in syms:
            print(f'{unit}: {name} ({sec}) missing in ours'); bad += 1; continue
        s, off, size = syms[name]
        mine = secs[s][off:off + size]
        if len(mine) != len(b):
            print(f'{unit}: {name} size {len(mine):#x} vs original {len(b):#x}'); bad += 1; continue
        diff = [i for i in range(len(b)) if mine[i] != b[i] and not any(r <= i < r + 4 for r in relocs)]
        if diff:
            print(f'{unit}: {name} differs at {[hex(i) for i in diff[:8]]}'); bad += 1
    print(f'{unit}: {"OK" if not bad else str(bad) + " problem(s)"}')
