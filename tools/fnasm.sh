#!/bin/sh
# Print one function's asm (instructions only): tools/fnasm.sh unit mangled_name
awk "/^\.fn $2,/{f=1} f{print} /^\.endfn $2\$/{f=0}" build/GMFE69/asm/game/$1.s | cut -c30- | grep -v '^$' | sed 's/^[0-9A-F ]*\*\/\t//'
