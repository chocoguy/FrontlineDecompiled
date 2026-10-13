#!/usr/bin/env python3

###
# Semantic spot-check for functionally equivalent code: compares, per
# function, the sequence of calls (bl targets), virtual-call slots
# (lwz r12, N(r12)) and loaded constants between the original and the
# source build, ignoring register allocation and scheduling.
#
#   python tools/callseq.py game/bsbifunc.cpp          # functions whose sequences differ
#   python tools/callseq.py game/bsbifunc.cpp -v       # + the differences
#   python tools/callseq.py game/bsbifunc.cpp -f Name  # only matching functions
#
# Uses the last build (run ninja or tools/fncheck.py first).
###

import argparse
import difflib
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from fncheck import diff_json, pretty  # noqa: E402

PATTERNS = [
    r"^bl (\S+)",                     # direct calls
    r"^lwz r12, (0x[0-9a-f]+)\(r12\)",  # virtual call slots
    r"^b (\S+)$",                     # tail calls to symbols (not local labels)
]


def sequence(sym: dict) -> list:
    out = []
    for ins in sym.get("instructions", []):
        text = ins.get("instruction", {}).get("formatted", "")
        for pat in PATTERNS:
            m = re.match(pat, text)
            if m:
                target = m.group(1)
                if pat.startswith("^b ") and re.match(r"^(0x|\.L)", target):
                    continue
                out.append(text if not pat.startswith("^b ") else "b " + target)
                break
    return out


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("unit")
    parser.add_argument("-v", "--verbose", action="store_true")
    parser.add_argument("-f", "--function")
    args = parser.parse_args()

    d = diff_json(args.unit)
    base = {s["name"]: s for s in d.get("right", {}).get("symbols", []) if s.get("instructions")}
    differing = 0
    checked = 0
    for s in d["left"]["symbols"]:
        if not s.get("instructions") or s.get("target_symbol") is None:
            continue
        name = s["name"]
        if args.function and args.function not in pretty(name):
            continue
        b = base.get(name)
        if b is None:
            continue
        checked += 1
        orig, src = sequence(s), sequence(b)
        if orig == src:
            continue
        differing += 1
        print(f"{pretty(name)}: {len(orig)} vs {len(src)} calls")
        if args.verbose:
            for line in difflib.unified_diff(orig, src, "original", "source", lineterm="", n=1):
                print("    " + line)
    print(f"{checked} functions checked, {differing} with different call sequences")


if __name__ == "__main__":
    main()
