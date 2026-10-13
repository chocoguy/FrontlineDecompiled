#!/usr/bin/env python3
# Finds 32-bit constants (lis+addi/subi/ori, subis/addis+cmplwi) that the
# original builds in a function but our object does not: catches CRCs
# mis-derived from the asm. Usage: python3 tools/constcheck.py <unit> ...
# Known false positives: switch range checks, lis reused across addi.
# Compare 32-bit constants built from two instructions (lis+addi/subi/ori,
# subis/addis+cmplwi) per function, original asm vs our object.
import re, subprocess, sys
from collections import Counter
def s16(v): return v-0x10000 if v&0x8000 else v
def parse_int(t): return int(t,16) if t.lower().startswith(('0x','-0x')) else int(t)
def consts(lines):
    out=Counter(); hi={}; cmpbase={}
    for l in lines:
        m=re.match(r'\s*(\w+)\.?\s+(.*)',l)
        if not m: continue
        op,args=m.group(1),[a.strip() for a in m.group(2).split(',')]
        try:
            if op=='lis' and not '@' in args[1]: hi[args[0]]=parse_int(args[1])<<16
            elif op in('addi','subi') and len(args)==3 and args[1] in hi and '@' not in args[2]:
                v=parse_int(args[2]); v=-v if op=='subi' else v
                out[(hi[args[1]]+v)&0xffffffff]+=1
            elif op=='ori' and args[1] in hi: out[(hi[args[1]]|parse_int(args[2]))&0xffffffff]+=1
            elif op=='subis': cmpbase[args[0]]=-(parse_int(args[2])<<16)
            elif op=='addis' and '@' not in args[2]: cmpbase[args[0]]=parse_int(args[2])<<16
            elif op in('cmplwi','cmpwi') and len(args)==2 and args[0] in cmpbase:
                out[(parse_int(args[1])-cmpbase[args[0]])&0xffffffff]+=1
            elif op in('cmplwi','cmpwi') and len(args)==3 and args[1] in cmpbase:
                out[(parse_int(args[2])-cmpbase[args[1]])&0xffffffff]+=1
        except ValueError: pass
    return out
def upath(unit):
    return unit if '/' in unit else f'game/{unit}'
def orig(unit):
    fns={};cur=None
    for l in open(f'build/GMFE69/asm/{upath(unit)}.s'):
        m=re.match(r'\.fn (\S+),',l)
        if m: cur=m.group(1); fns[cur]=[]; continue
        if l.startswith('.endfn'): cur=None; continue
        if cur:
            l=re.sub(r'/\*.*?\*/','',l); fns[cur].append(l)
    return {k:consts(v) for k,v in fns.items()}
def ours(unit):
    txt=subprocess.run(['build/binutils/powerpc-eabi-objdump','-d',f'build/GMFE69/src/{upath(unit)}.o'],capture_output=True,text=True).stdout
    fns={};cur=None
    for l in txt.splitlines():
        m=re.match(r'[0-9a-f]+ <(\S+)>:',l)
        if m: cur=m.group(1); fns[cur]=[]; continue
        if cur:
            p=l.split('\t')
            if len(p)>=3: fns[cur].append(p[2]+' '+(p[3] if len(p)>3 else ''))
    return {k:consts(v) for k,v in fns.items()}
for unit in sys.argv[1:]:
    o=orig(unit); u=ours(unit)
    for fn,c in o.items():
        if fn not in u: continue
        big={k for k in c if k>0xffff and k<0xffff0000}
        miss={hex(k) for k in big if k not in u[fn]}
        if miss: print(unit,fn,'missing in ours:',sorted(miss))
