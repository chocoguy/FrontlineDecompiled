#!/usr/bin/env python3
# print vtable slot offsets for a class: vt.py ClassName
import sys, glob, re, subprocess
cls=sys.argv[1]
mang='__vt__%d%s'%(len(cls),cls)
for f in glob.glob('build/GMFE69/asm/**/*.s',recursive=True):
    s=open(f).read()
    m=re.search(r'^\.obj %s, \w+\n(.*?)^\.endobj'%re.escape(mang),s,re.S|re.M)
    if not m: continue
    off=0
    for l in m.group(1).split('\n'):
        l=l.strip()
        if l.startswith('.4byte') or l.startswith('.rel'):
            v=l.split(None,1)[1].split(',')[0]
            try: d=subprocess.run(['python3','tools/cwdemangle.py',v],capture_output=True,text=True).stdout.strip()
            except Exception: d=''
            print(hex(off), v, d if d and d!=v else '')
            off+=4
    break
