#!/bin/bash
# Compile one translation unit that includes every header in include/game with the
# game's compiler flags, to catch conflicts between hand-edited headers
# (gen_headers.py --check only checks freshly generated copies).
cd "$(dirname "$0")/.."
mkdir -p build/scratch
TU=build/scratch/all_headers.cpp
for h in include/game/*.h; do echo "#include \"${h#include/}\""; done > $TU
build/tools/wibo build/compilers/GC/1.3.2/mwcceppc.exe -nodefaults -proc gekko -align powerpc \
  -enum int -fp hardware -O4,p -Cpp_exceptions on -RTTI on -maxerrors 20 -nosyspath -i include \
  -c $TU -o build/scratch/all_headers.o && echo "all headers OK"
