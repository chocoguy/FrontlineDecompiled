#!/bin/sh
###
# Compile one object and print its errors compactly.
#
#   tools/cc.sh build/GMFE69/src/game/X.o [max-lines]
###
cd "$(dirname "$0")/.."
ninja "$1" 2>&1 | grep -v "^build/tools/wibo" | grep -E "In:|Error|Warning|^#   [^ ]|line" | grep -v "Too many\|User break\|^# ---" | head -"${2:-12}"
