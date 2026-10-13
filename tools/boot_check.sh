#!/bin/bash
###
# Boot the current build in Dolphin with an isolated profile for N seconds and
# compare its OSReport log with a reference log from the matching build.
#
#   tools/boot_check.sh <profile-dir> <reference.log> <out.log> [seconds] [full]
#
# The profile dir needs a Logger.ini enabling OSREPORT + FileMon with
# WriteToFile (see the dolphin-test-loop notes). Logs are normalized (times,
# heap addresses, movie stats dropped) and compared up to the intro movie, or
# with `full`, over the whole run (the attract demo loads level 6_1 at ~90s).
###
set -u
U=$1; REF=$2; OUT=$3; SECS=${4:-45}; MODE=${5:-intro}
cd "$(dirname "$0")/.."
rm -f "$U/Logs/dolphin.log"
python3 tools/run_dolphin.py --stage-only >/dev/null || exit 1
/Applications/Dolphin.app/Contents/MacOS/Dolphin -u "$U" -b -e build/GMFE69/disc/sys/main.dol >/dev/null 2>&1 &
PID=$!
for i in $(seq 1 "$SECS"); do sleep 1; done
kill $PID 2>/dev/null; sleep 1
cp "$U/Logs/dolphin.log" "$OUT"
echo "exceptions: $(grep -ci exception "$OUT")"
norm() {
  sed 's/^[0-9:]* //' "$1" | grep -v '^\s*$' | sed 's/.*\]: //' \
    | grep -v 'segment\|Clearing I\|Moh2RelGC.elf\|Booting from disc' \
    | sed -E 's/(Arena : |MEM_init\(\) return |Durring Moive ).*/\1<n>/' \
    | grep -vE '^(Frames|Min FrameRate|Ave FrameRate|CPU Time|TIMER Time|Durring Moive)' \
    | if [ "$MODE" = full ]; then cat; else sed '/intro.mpc/q'; fi
}
if diff <(norm "$REF") <(norm "$OUT"); then
  echo "IDENTICAL ($MODE, $(norm "$OUT" | wc -l | tr -d ' ') lines)"
else
  exit 2
fi
