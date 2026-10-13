#!/usr/bin/env python3
# Shows where MWCC actually places bitfields (byte, bit MSB-first, length):
# MWCC misplaces some runs (path.h PATHFINDNODE), so check against the asm.
# usage: python3 tools/bitprobe.py game/path.h PATHFINDNODE:fadeIn,fadeOut [Struct:f,...]
import sys, subprocess, re, os
S = 'build/scratch'
hdr = sys.argv[1]
src = ['#include "%s"' % hdr]
names = []
for spec in sys.argv[2:]:
    st, fl = spec.split(':')
    for f in fl.split(','):
        n = '%s_%s' % (st, f); names.append(n)
        src.append('extern "C" void %s(%s* p){ p->%s = ~0; }' % (n, st, f))
open(S + '/bp.cpp', 'w').write('\n'.join(src) + '\n')
r = subprocess.run(['build/tools/wibo', 'build/compilers/GC/1.3.2/mwcceppc.exe', '-nodefaults', '-proc', 'gekko', '-align', 'powerpc', '-enum', 'int', '-fp', 'hardware', '-O4,p', '-i', 'include', '-i', 'build/GMFE69/include', '-lang=c++', '-c', S + '/bp.cpp', '-o', S + '/bp.o'], capture_output=True, text=True)
if r.returncode: print(r.stdout, r.stderr); sys.exit(1)
d = subprocess.run(['build/binutils/powerpc-eabi-objdump', '-d', S + '/bp.o'], capture_output=True, text=True).stdout
for blk in re.split(r'\n(?=[0-9a-f]+ <)', d):
    m = re.match(r'[0-9a-f]+ <(\w+)>', blk)
    if not m: continue
    ld = re.search(r'\t(lbz|lhz|lwz)\s+r\d+,(\d+)\(r3\)', blk)
    ri = re.search(r'rlwimi\s+r\d+,r\d+,(\d+),(\d+),(\d+)', blk)
    ori = re.search(r'\t(ori|oris)\s+r\d+,r\d+,(\d+)', blk)
    st = re.search(r'\t(stb|sth|stw)\s+r\d+,(\d+)\(r3\)', blk)
    w = {'lbz': 8, 'lhz': 16, 'lwz': 32, 'stb': 8, 'sth': 16, 'stw': 32}
    if ri and ld:
        width = w[ld.group(1)]; mb, me = int(ri.group(2)), int(ri.group(3))
        print('%-28s off 0x%X  %2d-bit unit  bits(msb0) %d..%d  -> byte 0x%X bit %d, len %d' % (m.group(1), int(ld.group(2)), width, mb - (32 - width), me - (32 - width), int(ld.group(2)) + (mb - (32 - width)) // 8, (mb - (32 - width)) % 8, me - mb + 1))
    else:
        print(m.group(1), blk.split('\n', 1)[1].strip().replace('\n', ' | ')[:200])
