#!/usr/bin/env python3

###
# Compare the relocations dtk inferred while splitting the DOL against the real
# relocations kept in the retail Moh2RelGC.elf.
#
# Non-matching code moves everything after it, so every pointer must be a
# relocation. A relocation the ELF has but dtk lacks is a value dtk hardcoded
# that will go stale when its target moves; a relocation dtk has but the ELF
# lacks is a constant or raw data dtk mistook for a pointer, which will get
# rewritten when its "target" moves. Fix either with block_relocations /
# add_relocations in config.yml.
#
# Run after `ninja` (it reads the split objects from the link command):
#   python tools/check_relocs.py
###

import argparse
import bisect
import collections
import re
import struct
import sys
from pathlib import Path

DTK_ROOT = Path(__file__).resolve().parent.parent

R_NAMES = {1: "addr32", 4: "lo", 5: "hi", 6: "ha", 10: "rel24", 11: "rel14", 109: "sda21"}


def read_elf(data: bytes):
    shoff = struct.unpack(">I", data[0x20:0x24])[0]
    shentsize, shnum, shstrndx = struct.unpack(">HHH", data[0x2E:0x34])
    secs = [struct.unpack(">10I", data[shoff + i * shentsize:shoff + i * shentsize + 40])
            for i in range(shnum)]

    def cstr(off: int) -> str:
        return data[off:data.index(b"\0", off)].decode("latin1")

    names = [cstr(secs[shstrndx][4] + s[0]) for s in secs]
    syms = []
    for s in secs:
        if s[1] == 2:  # SYMTAB
            strtab = secs[s[6]]
            for j in range(s[5] // 16):
                nm, value, _size, _info, _other, shndx = struct.unpack(
                    ">IIIBBH", data[s[4] + j * 16:s[4] + j * 16 + 16])
                syms.append((cstr(strtab[4] + nm), value, shndx))
    return secs, names, syms


def relocs(data: bytes, secs):
    """Yield (target section index, offset, type) for every RELA entry."""
    for s in secs:
        if s[1] == 4:  # RELA
            for j in range(s[5] // 12):
                off, info, _addend = struct.unpack(">IIi", data[s[4] + j * 12:s[4] + j * 12 + 12])
                yield s[7], off, info & 0xFF


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("-v", "--version", default="GMFE69")
    parser.add_argument("--orig", type=Path,
                        help="retail ELF (default: orig/<version>/Moh2RelGC.elf)")
    parser.add_argument("--all", action="store_true", help="list every mismatch")
    args = parser.parse_args()

    config_dir = DTK_ROOT / "config" / args.version
    build_dir = DTK_ROOT / "build" / args.version
    orig = args.orig or DTK_ROOT / "orig" / args.version / "Moh2RelGC.elf"

    # Real relocations. In an executable, r_offset is already a virtual address.
    data = orig.read_bytes()
    secs, _, _ = read_elf(data)
    real = {off & ~3: t for _, off, t in relocs(data, secs)}

    # Unique symbol names -> address, used to place objects with split sections.
    addr, dup = {}, set()
    for line in open(config_dir / "symbols.txt"):
        m = re.match(r"^(\S+) = \S+:0x(\w+);", line)
        if m:
            if m.group(1) in addr:
                dup.add(m.group(1))
            addr[m.group(1)] = int(m.group(2), 16)
    for name in dup:
        del addr[name]

    # Split start addresses per unit and section.
    ranges = collections.defaultdict(lambda: collections.defaultdict(list))
    unit = None
    for line in open(config_dir / "splits.txt"):
        m = re.match(r"^(\S+):$", line)
        if m and m.group(1) != "Sections":
            unit = re.sub(r"\.[A-Za-z]+$", "", m.group(1))
            continue
        m = re.match(r"\s+(\S+)\s+start:0x(\w+)", line)
        if m and unit:
            ranges[unit][m.group(1)].append(int(m.group(2), 16))

    # Only the objects actually linked (build/ can hold stale ones).
    ninja = (DTK_ROOT / "build.ninja").read_text()
    start = re.search(rf"^build build/{args.version}/Moh2RelGC\.elf( \| \S+)?: link", ninja, re.M).start()
    block = ninja[start:ninja.index("\n\n", start)]
    # (kind, unit): dtk's split objects live in obj/, compiled sources in src/
    objects = re.findall(rf"build/{args.version}/(obj|src)/(\S+)\.o", block)
    if not objects:
        sys.exit("No linked objects found in build.ninja; run configure.py and ninja first")

    dtk, owner, unplaced = {}, {}, 0
    for _kind, obj in objects:
        # always dtk's split object: this checks dtk's inferred relocations, and
        # objects compiled from source may be laid out differently
        data = (build_dir / "obj" / f"{obj}.o").read_bytes()
        osecs, onames, osyms = read_elf(data)
        anchors = collections.defaultdict(list)
        for name, value, shndx in osyms:
            if name in addr and 0 < shndx < len(osecs):
                anchors[shndx].append((value, addr[name]))
        for k in anchors:
            anchors[k].sort()
        for tgt, off, t in relocs(data, osecs):
            starts = ranges.get(obj, {}).get(onames[tgt], [])
            if len(starts) == 1:
                a = starts[0] + off
            else:
                anc = anchors.get(tgt, [])
                i = bisect.bisect_right([x[0] for x in anc], off) - 1
                if i < 0:
                    unplaced += 1
                    continue
                a = anc[i][1] + off - anc[i][0]
            dtk[a & ~3] = t
            owner[a & ~3] = obj

    only_real = sorted(set(real) - set(dtk))
    only_dtk = sorted(set(dtk) - set(real))
    print(f"ELF relocations: {len(real)}  dtk relocations: {len(dtk)}  unplaced: {unplaced}")

    owned = sorted(owner.items())
    keys = [k for k, _ in owned]

    def who(a: int) -> str:
        if a in owner:
            return owner[a]
        i = bisect.bisect_right(keys, a) - 1
        return owned[i][1] if i >= 0 else "?"

    def report(title: str, addrs, types) -> None:
        print(f"\n{title}: {len(addrs)}")
        limit = None if args.all else 20
        for a in addrs[:limit]:
            print(f"  {a:08X} {R_NAMES.get(types[a], types[a]):6} {who(a)}")
        if limit and len(addrs) > limit:
            print(f"  ... {len(addrs) - limit} more (use --all)")

    report("In ELF, missing from dtk (hardcoded; add_relocations)", only_real, real)
    report("In dtk, not in ELF (false pointer?; block_relocations)", only_dtk, dtk)
    # Known benign: branches to functions in the same unit, which the retail
    # linker resolved without keeping a relocation.
    print("\nNote: rel24/rel14 entries in the second list are usually same-unit branches and harmless.")
    sys.exit(1 if only_real or unplaced else 0)


if __name__ == "__main__":
    main()
