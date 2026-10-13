#!/usr/bin/env python3

###
# Catch regressions in already-decompiled units: compare every function's
# objdiff match against a saved baseline and report any that dropped.
#
#   python tools/regress.py            # compare against docs/match_baseline.json
#   python tools/regress.py --update   # accept the current matches as the baseline
#
# A header change (a struct growing, a field moving) silently changes code in
# every unit that uses it; matches dropping in units nobody touched is the
# signal. Run it after editing shared headers, before handing out a build.
###

import argparse
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BASELINE = ROOT / "docs" / "match_baseline.json"


def current() -> dict:
    r = subprocess.run(["ninja", "build/GMFE69/report.json"], cwd=ROOT, capture_output=True,
                       text=True)
    if r.returncode != 0:
        sys.exit(r.stdout + r.stderr)
    report = json.loads((ROOT / "build/GMFE69/report.json").read_text())
    out = {}
    for unit in report["units"]:
        for f in unit.get("functions", []):
            pct = f.get("fuzzy_match_percent")
            if pct is None or "metadata" not in f:
                continue
            out[f"{unit['name']}::{f['name']}"] = round(pct, 2)
    return out


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--update", action="store_true", help="save the current matches")
    args = parser.parse_args()

    now = current()
    if args.update or not BASELINE.exists():
        BASELINE.write_text(json.dumps(now, indent=0, sort_keys=True) + "\n")
        print(f"baseline saved ({len(now)} functions)")
        return
    base = json.loads(BASELINE.read_text())
    dropped = [(k, base[k], now[k]) for k in sorted(base) if k in now and now[k] < base[k]]
    gone = [k for k in sorted(base) if k not in now]
    for k, a, b in dropped:
        print(f"DROPPED {a:6.2f} -> {b:6.2f}  {k}")
    for k in gone:
        print(f"MISSING {k}")
    improved = sum(1 for k in now if k in base and now[k] > base[k])
    print(f"{len(dropped)} dropped, {len(gone)} missing, {improved} improved, "
          f"{len(now) - len(base)} new functions")
    sys.exit(1 if dropped or gone else 0)


if __name__ == "__main__":
    main()
