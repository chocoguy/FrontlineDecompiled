#!/usr/bin/env python3
###
# For each function of the given units, compares which argument registers
# (r3-r10, f1-f8) the original reads before writing them with ours. A
# mismatch usually means a wrong signature: static vs member, or a missing
# or extra parameter. Straight-line approximation (ignores branches).
#
#   python3 tools/argregs.py <unit> ...
###
import re, subprocess, sys

ARGS = ["r%d" % i for i in range(3, 11)] + ["f%d" % i for i in range(1, 9)]

def regs(s):
    return re.findall(r"\b([rf]\d+)\b", s)

def first_reads(lines):
    written = set()
    read = set()
    for l in lines:
        m = re.match(r"\s*([a-z_.]+)\s*(.*)", l)
        if not m:
            continue
        op, a = m.group(1).rstrip("."), m.group(2)
        if op in ("bl", "b", "blr", "bctr", "bctrl"):
            if op != "bl":
                pass
            break
        ops = [x.strip() for x in a.split(",")]
        rs = [regs(x) for x in ops]
        stores = op.startswith("st") or op.startswith("psq_st") or op.startswith("cmp") or op in ("mtctr", "mtlr", "mtspr")
        if stores:
            srcs = sum(rs, [])
            dst = []
        else:
            dst = rs[0] if rs else []
            srcs = sum(rs[1:], [])
        if op.startswith("st") and op.endswith("u"):
            pass
        for r in srcs:
            if r in ARGS and r not in written:
                read.add(r)
        for r in dst:
            written.add(r)
    return read

def orig(unit):
    fns, cur = {}, None
    for l in open("build/GMFE69/asm/game/%s.s" % unit):
        m = re.match(r'\.fn "?([^",]+)"?,', l)
        if m:
            cur = m.group(1); fns[cur] = []; continue
        if l.startswith(".endfn"):
            cur = None; continue
        if cur:
            l = re.sub(r"/\*.*?\*/", "", l).strip()
            if l and not l.startswith("."):
                fns[cur].append(l)
    return fns

def ours(unit):
    txt = subprocess.run(["build/binutils/powerpc-eabi-objdump", "-d", "build/GMFE69/src/game/%s.o" % unit],
                         capture_output=True, text=True).stdout
    fns, cur = {}, None
    for l in txt.splitlines():
        m = re.match(r"[0-9a-f]+ <(\S+)>:", l)
        if m:
            cur = m.group(1); fns[cur] = []; continue
        if cur:
            p = l.split("\t")
            if len(p) >= 3:
                fns[cur].append(p[2] + " " + (p[3] if len(p) > 3 else ""))
    return fns

for unit in sys.argv[1:]:
    o, u = orig(unit), ours(unit)
    for fn, lines in o.items():
        if fn not in u:
            continue
        a, b = first_reads(lines), first_reads(u[fn])
        ga = {r for r in a if r[0] == "r"}
        gb = {r for r in b if r[0] == "r"}
        fa = {r for r in a if r[0] == "f"}
        fb = {r for r in b if r[0] == "f"}
        if (ga and gb and max(ga, key=lambda r: int(r[1:])) != max(gb, key=lambda r: int(r[1:]))) or \
           (ga and not gb) or (gb and not ga) or (fa and fb and max(fa) != max(fb)):
            print("%s %s: original reads %s, ours %s" % (unit, fn, sorted(a), sorted(b)))
