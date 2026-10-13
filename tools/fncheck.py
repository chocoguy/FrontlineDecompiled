#!/usr/bin/env python3

###
# Compile a unit's source and compare it function by function against the
# original with objdiff.
#
#   python tools/fncheck.py game/Screen.cpp            # summary table
#   python tools/fncheck.py game/Screen.cpp --diff     # + diffs of functions below 100%
#   python tools/fncheck.py game/Screen.cpp -f Draw    # diff functions matching a substring
#
# Functions not defined in the source are listed as "asm" (still taken from the
# original through the unit's partial object).
###

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from cwdemangle import demangle, full_signature  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
VERSION = "GMFE69"


def pretty(name: str) -> str:
    d = demangle(name)
    return full_signature(d) if d else name


def compile_unit(unit: str) -> bool:
    stem = re.sub(r"\.[A-Za-z]+$", "", unit)
    obj = f"build/{VERSION}/src/{stem}.o"
    r = subprocess.run(["ninja", obj], cwd=ROOT, capture_output=True, text=True)
    if r.returncode != 0:
        out = r.stdout + r.stderr
        # keep compiler messages, drop ninja noise
        lines = [l for l in out.splitlines() if not l.startswith(("[", "ninja:", "FAILED"))]
        print("\n".join(lines[:60]))
        return False
    return True


def diff_json(unit: str) -> dict:
    stem = re.sub(r"\.[A-Za-z]+$", "", unit)
    r = subprocess.run([str(ROOT / "build/tools/objdiff-cli"), "diff", "-p", ".", "-u",
                        f"Moh2RelGC/{stem}", "-o", "-", "--format", "json"],
                       cwd=ROOT, capture_output=True, text=True)
    if r.returncode != 0:
        sys.exit(r.stderr)
    return json.loads(r.stdout)


def show_diff(target: dict, base: dict) -> None:
    ti, bi = target.get("instructions", []), base.get("instructions", [])
    width = 46
    for k in range(max(len(ti), len(bi))):
        t = ti[k] if k < len(ti) else {}
        b = bi[k] if k < len(bi) else {}
        tf = t.get("instruction", {}).get("formatted", "")
        bf = b.get("instruction", {}).get("formatted", "")
        kind = t.get("diff_kind") or b.get("diff_kind") or ""
        mark = " " if not kind else {"DIFF_REPLACE": "|", "DIFF_DELETE": "<", "DIFF_INSERT": ">",
                                     "DIFF_OP_MISMATCH": "|", "DIFF_ARG_MISMATCH": "~"}.get(kind, "*")
        print(f"    {tf[:width]:<{width}} {mark} {bf[:width]}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("unit")
    parser.add_argument("--diff", action="store_true", help="show diffs of functions below 100%%")
    parser.add_argument("-f", "--function", help="show diffs for functions containing this text")
    parser.add_argument("--no-compile", action="store_true")
    args = parser.parse_args()

    if not args.no_compile and not compile_unit(args.unit):
        sys.exit(1)
    d = diff_json(args.unit)
    target_syms = d["left"]["symbols"]
    base_syms = d.get("right", {}).get("symbols", [])
    rows = []
    for s in target_syms:
        if not s.get("instructions"):
            continue
        name = s["name"]
        matched = s.get("target_symbol") is not None
        pct = s.get("match_percent") if matched else None
        rows.append((name, int(s.get("size", 0)), pct, s))
    done = [r for r in rows if r[2] is not None]
    exact = [r for r in done if r[2] == 100.0]
    print(f"{args.unit}: {len(done)}/{len(rows)} functions in source, {len(exact)} at 100%")
    for name, size, pct, s in rows:
        status = "asm " if pct is None else f"{pct:5.1f}%"
        print(f"  {status}  0x{size:<5X} {pretty(name)}")
    for name, size, pct, s in rows:
        want = (args.function and args.function in pretty(name)) or \
            (args.diff and pct is not None and pct < 100.0)
        if not want or pct is None:
            continue
        base = next((b for b in base_syms if b.get("name") == name and b.get("instructions")), {})
        print(f"\n== {pretty(name)}  {pct:.1f}%   (original | source)")
        show_diff(s, base)


if __name__ == "__main__":
    main()
