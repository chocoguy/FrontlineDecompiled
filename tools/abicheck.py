#!/usr/bin/env python3

###
# Flag calls whose argument setup differs between the original and our code.
#
#   python tools/abicheck.py                 # every decompiled unit
#   python tools/abicheck.py game/X.cpp ...  # some units
#
# For each function below 100% match, every call (bl) is paired by target
# with the original's, and the argument registers written since the previous
# call (r3-r10, f1-f8) are compared. A mismatch usually means a wrong
# prototype: a missing hidden return pointer, a wrong parameter type or
# count. It is noisy (scheduler temporaries also count as writes), so read
# each hit against the diff (fncheck -f).
#
# With --consts, only argument registers that one side sets to a constant
# (`li rX, N`) and the other to a different constant or a computed value are
# reported: far less noise, and it catches NULL passed where the original
# passes a value.
#
# It also flags constants the original loads into r3 (`li r3, X`) that ours
# never does: a return type declared narrower or wider than the original's
# (e.g. u16 0xFFFF instead of int -1, which callers test with `< 0`).
###

import json
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ARG_REGS = {f"r{i}" for i in range(3, 11)} | {f"f{i}" for i in range(1, 9)}
WRITERS = re.compile(r"^(addi|addis|li|lis|mr|lwz|lwzx|lhz|lha|lbz|lfs|lfd|fmr|fmuls|fadds|fsubs|"
                     r"fdivs|fneg|fmadds|fmsubs|fnmsubs|fnmadds|slwi|srwi|srawi|clrlwi|extsh|"
                     r"extsb|add|subf|subi|ori|rlwinm|neg|or|and|xoris|mulli|frsp|fctiwz|lfsx|lwzu)\b")


def units() -> list:
    cfg = (ROOT / "configure.py").read_text()
    return [u for s, u in re.findall(r'Object\((\w+), "([^"]+)"', cfg)
            if s in ("Equivalent", "Matching")]


def calls(instrs: list) -> list:
    """[(target, frozenset(arg regs written since previous call))]"""
    out, written = [], set()
    for ins in instrs:
        text = ins.get("instruction", {}).get("formatted", "")
        if not text:
            continue
        if text.startswith("bl "):
            out.append((text.split()[1], frozenset(written & ARG_REGS)))
            written = set()
            continue
        m = WRITERS.match(text)
        if m:
            dst = text.split()[1].rstrip(",")
            written.add(dst)
    return out


def call_values(instrs: list) -> list:
    """[(target, {arg reg: constant string or None})] with the last write to
    each argument register since the previous call."""
    out, values = [], {}
    for ins in instrs:
        text = ins.get("instruction", {}).get("formatted", "")
        if not text:
            continue
        if text.startswith("bl "):
            out.append((text.split()[1], {k: v for k, v in values.items() if k in ARG_REGS}))
            values = {}
            continue
        if WRITERS.match(text):
            parts = text.replace(",", " ").split()
            dst = parts[1]
            if parts[0] == "li":
                values[dst] = parts[2]
            elif parts[0] == "mr" and parts[2] in values:
                values[dst] = values[parts[2]]
            else:
                values[dst] = None
    return out


def r3_constants(instrs: list) -> set:
    """Constants loaded with `li r3, X` that reach a `blr` still in r3."""
    texts = [ins.get("instruction", {}).get("formatted", "") for ins in instrs]
    index_of = {}
    for i, ins in enumerate(instrs):
        index_of[int(ins.get("instruction", {}).get("address", "0") or 0)] = i
    out = set()
    for i, text in enumerate(texts):
        if not text.startswith("li r3, "):
            continue
        j, steps = i + 1, 0
        while j < len(texts) and steps < 40:
            t = texts[j]
            steps += 1
            if t == "blr":
                out.add(text.split(", ", 1)[1])
                break
            parts = t.replace(",", " ").split()
            if not parts:
                j += 1
                continue
            op = parts[0]
            if op == "b" and len(parts) > 1 and parts[1].startswith("0x"):
                j = index_of.get(int(parts[1], 16), len(texts))
                continue
            # Stop at calls, other branches, or anything writing r3.
            if op.startswith("b") or (len(parts) > 1 and parts[1] == "r3"):
                break
            j += 1
    return out


def check(unit: str, consts: bool = False) -> int:
    stem = re.sub(r"\.[A-Za-z]+$", "", unit)
    r = subprocess.run([str(ROOT / "build/tools/objdiff-cli"), "diff", "-p", ".", "-u",
                        f"Moh2RelGC/{stem}", "-o", "-", "--format", "json"],
                       cwd=ROOT, capture_output=True, text=True)
    if r.returncode != 0:
        print(f"{unit}: objdiff failed")
        return 1
    d = json.loads(r.stdout)
    # objdiff: "left" is the original (target), "right" our build.
    built = {s["name"]: s for s in d.get("right", {}).get("symbols", []) if s.get("instructions")}
    problems = 0
    for orig in d["left"]["symbols"]:
        if not orig.get("instructions") or orig.get("match_percent", 100.0) >= 100.0:
            continue
        s = built.get(orig["name"])
        if s is None:
            continue
        b = orig
        missing = r3_constants(b["instructions"]) - r3_constants(s["instructions"])
        if missing:
            problems += 1
            print(f"{unit}: {s['name']}: original returns/passes r3 = {sorted(missing)}, ours never does")
        if consts:
            pending = call_values(b["instructions"])
            for target, vals in call_values(s["instructions"]):
                match = next((t for t in pending if t[0] == target), None)
                if match is None:
                    continue
                pending.remove(match)
                for reg in sorted(set(vals) & set(match[1])):
                    a, o = vals[reg], match[1][reg]
                    if a != o and (a is not None or o is not None):
                        problems += 1
                        print(f"{unit}: {s['name']}: call {target}: {reg} original "
                              f"{o if o is not None else 'value'}, ours {a if a is not None else 'value'}")
            continue
        ours, theirs = calls(s["instructions"]), calls(b["instructions"])
        pending = list(theirs)
        for target, regs in ours:
            match = next((t for t in pending if t[0] == target), None)
            if match is None:
                continue
            pending.remove(match)
            if regs != match[1]:
                problems += 1
                print(f"{unit}: {s['name']}: call {target}: original sets "
                      f"{sorted(match[1])}, ours {sorted(regs)}")
    return problems


def main() -> None:
    args = [a for a in sys.argv[1:] if a != "--consts"]
    consts = "--consts" in sys.argv[1:]
    todo = args or units()
    total = sum(check(u, consts) for u in todo)
    print(f"{total} call(s) with different argument setup")


if __name__ == "__main__":
    main()
