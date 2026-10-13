#!/usr/bin/env python3

###
# Build the "rest" object for a partially decompiled unit.
#
# Input: dtk's split object for the unit (all original code and data) and the
# object compiled from src/<unit>.cpp (the functions decompiled so far).
# Output: a copy of the split object where every symbol the compiled source
# also defines becomes a local "old copy", and every reference to it (from
# code, vtables, tables...) is redirected to the source's definition.
#
# Exception tables (extab/extabindex) keep pointing at the old copies, so they
# still describe the code they were generated for. Old copies lose dtk's
# "force active" flag, so the linker dead-strips them and their table entries. Local symbols of the split
# object that the source refers to by name are made global so they resolve.
#
# The split object is used instead of reassembling dtk's .s, because GNU as
# turns relocations against local symbols into section offsets, which breaks
# CodeWarrior's linker handling of exception tables.
#
#   python tools/partial_obj.py SPLIT_OBJ SRC_OBJ -o OUT_OBJ
###

import argparse
import struct
import sys
from pathlib import Path
from typing import Dict, List, Set, Tuple

SHT_SYMTAB, SHT_STRTAB, SHT_RELA = 2, 3, 4
STB_LOCAL, STB_GLOBAL, STB_WEAK = 0, 1, 2
SHN_UNDEF, SHN_ABS = 0, 0xFFF1
COMMENT_HEADER = 0x2C
KEEP_TARGETS = ("extab", "extabindex")


class Elf:
    def __init__(self, data: bytes):
        self.data = data
        self.header = bytearray(data[:0x34])
        shoff = struct.unpack(">I", data[0x20:0x24])[0]
        shentsize, shnum, self.shstrndx = struct.unpack(">HHH", data[0x2E:0x34])
        self.sh = [list(struct.unpack(">10I", data[shoff + i * shentsize:shoff + (i + 1) * shentsize]))
                   for i in range(shnum)]
        self.body = [data[s[4]:s[4] + s[5]] if s[1] != 8 else b"" for s in self.sh]
        names = self.body[self.shstrndx]
        self.names = [names[s[0]:names.index(b"\0", s[0])].decode() for s in self.sh]
        self.symtab = next(i for i, s in enumerate(self.sh) if s[1] == SHT_SYMTAB)
        strtab = self.body[self.sh[self.symtab][6]]
        raw = self.body[self.symtab]
        self.syms = []
        for j in range(len(raw) // 16):
            nm, value, size, info, other, shndx = struct.unpack(">IIIBBH", raw[j * 16:j * 16 + 16])
            name = strtab[nm:strtab.index(b"\0", nm)].decode("latin1")
            self.syms.append([name, value, size, info, other, shndx])
        self.relas: Dict[int, List[List[int]]] = {}
        for i, s in enumerate(self.sh):
            if s[1] == SHT_RELA:
                raw = self.body[i]
                self.relas[i] = [list(struct.unpack(">IIi", raw[k:k + 12])) for k in range(0, len(raw), 12)]
        self.comment = self.names.index(".comment") if ".comment" in self.names else None

    def defined(self) -> Dict[str, int]:
        """name -> binding for symbols defined (not undefined, not section/file symbols)."""
        out = {}
        for name, value, size, info, other, shndx in self.syms[1:]:
            if name and shndx != SHN_UNDEF and (info & 15) in (0, 1, 2):  # NOTYPE/OBJECT/FUNC
                out[name] = info >> 4
        return out

    def undefined(self) -> Set[str]:
        return {s[0] for s in self.syms[1:] if s[5] == SHN_UNDEF and s[0]}

    def write(self) -> bytes:
        out = bytearray(self.header)
        offsets = []
        for i, s in enumerate(self.sh):
            if i == 0 or s[1] == 8:
                offsets.append(len(out) if i else 0)
                continue
            align = max(s[8], 1)
            while len(out) % align:
                out.append(0)
            offsets.append(len(out))
            out += self.body[i]
        while len(out) % 4:
            out.append(0)
        shoff = len(out)
        for i, s in enumerate(self.sh):
            s = list(s)
            if i:
                s[4] = offsets[i]
                if s[1] != 8:
                    s[5] = len(self.body[i])
            out += struct.pack(">10I", *s)
        out[0x20:0x24] = struct.pack(">I", shoff)
        return bytes(out)


def containing_symbol(syms: List[list], sec: int, offset: int) -> int:
    """Index of the defined symbol in section `sec` that covers `offset` (or -1)."""
    best, best_val = -1, -1
    for i, s in enumerate(syms):
        if s[5] == sec and (s[3] & 15) in (1, 2) and best_val <= s[1] <= offset < s[1] + max(s[2], 1):
            best, best_val = i, s[1]
    return best


def unflag_dead_locals(split: Elf, syms: List[list], comment: List[bytes], old: Set[int]) -> None:
    """Clear "force active" on local symbols used only by old copies.

    Exception index entries belong to the function they describe and their
    extab entry belongs to them, so those follow the function.
    """
    referrers: Dict[int, Set[int]] = {}
    for sec, relas in split.relas.items():
        target_sec = split.sh[sec][7]
        for r in relas:
            sym = r[1] >> 8
            owner = containing_symbol(syms, target_sec, r[0])
            referrers.setdefault(sym, set()).add(owner)
    dead = set(old)
    idx_sec = split.names.index("extabindex") if "extabindex" in split.names else None
    changed = True
    while changed:
        changed = False
        for i, s in enumerate(syms):
            if i in dead or s[3] >> 4 != STB_LOCAL or (s[3] & 15) not in (1, 2) or s[5] == SHN_UNDEF:
                continue
            if s[5] == idx_sec:
                # an index entry is dead when the function it describes is
                func = next((r[1] >> 8 for sec, relas in split.relas.items()
                             if split.sh[sec][7] == idx_sec for r in relas if r[0] == s[1]), None)
                if func in dead:
                    dead.add(i)
                    changed = True
                continue
            refs = referrers.get(i, set())
            if refs and refs <= dead:
                dead.add(i)
                changed = True
    for i in dead:
        e = bytearray(comment[i])
        e[5] &= ~0x08
        comment[i] = bytes(e)


def build(split: Elf, src: Elf) -> Tuple[bytes, Dict[str, int]]:
    # Only the source's global/weak symbols replace the original's: a local
    # (file-static or header static like dwi::dwi_prime_list) can't be
    # referenced from the rest object, which keeps using its own copy.
    replaced = {n for n, b in src.defined().items() if b != STB_LOCAL}
    wanted_local = src.undefined()
    stats = {"replaced": 0, "promoted": 0, "kept_functions": 0}
    keep_targets = {i for i, n in enumerate(split.names) if n in KEEP_TARGETS}

    syms = [list(s) for s in split.syms]
    comment = None
    if split.comment is not None:
        c = split.body[split.comment]
        comment = [c[COMMENT_HEADER + k * 8:COMMENT_HEADER + k * 8 + 8] for k in range(len(syms))]

    # old copies of replaced symbols become local; new undefined globals take their name
    redirect: Dict[int, int] = {}
    extra: List[list] = []
    seen_new: Dict[str, int] = {}
    for idx, s in enumerate(syms):
        name, shndx, bind, typ = s[0], s[5], s[3] >> 4, s[3] & 15
        if idx == 0 or not name or shndx == SHN_UNDEF or typ > 2:
            continue
        if name in replaced:
            s[0] = name + "$old"
            s[3] = (STB_LOCAL << 4) | typ
            if comment is not None:
                # drop dtk's "force active" flag so the linker dead-strips the old copy
                e = bytearray(comment[idx])
                e[5] &= ~0x08
                comment[idx] = bytes(e)
            if name not in seen_new:
                seen_new[name] = len(syms) + len(extra)
                extra.append([name, 0, 0, (STB_GLOBAL << 4) | 0, 0, SHN_UNDEF])
            redirect[idx] = seen_new[name]
            stats["replaced"] += 1
        elif bind == STB_LOCAL and name in wanted_local:
            s[3] = (STB_GLOBAL << 4) | typ
            stats["promoted"] += 1
        elif typ == 2:
            stats["kept_functions"] += 1
    syms += extra
    if comment is not None:
        comment += [bytes(8)] * len(extra)
        unflag_dead_locals(split, syms, comment, set(redirect))

    # ELF wants locals first: reorder and remap indices
    order = [0] + [i for i in range(1, len(syms)) if syms[i][3] >> 4 == STB_LOCAL] \
        + [i for i in range(1, len(syms)) if syms[i][3] >> 4 != STB_LOCAL]
    new_index = {old: new for new, old in enumerate(order)}
    first_global = next((k for k, i in enumerate(order) if k and syms[i][3] >> 4 != STB_LOCAL), len(order))

    for sec, relas in split.relas.items():
        target = split.sh[sec][7]
        for r in relas:
            sym, typ = r[1] >> 8, r[1] & 0xFF
            if sym in redirect and target not in keep_targets:
                sym = redirect[sym]
            r[1] = (new_index[sym] << 8) | typ

    strtab = bytearray(b"\0")
    raw = bytearray()
    for i in order:
        name, value, size, info, other, shndx = syms[i]
        off = 0
        if name:
            off = len(strtab)
            strtab += name.encode("latin1") + b"\0"
        raw += struct.pack(">IIIBBH", off, value, size, info, other, shndx)
    split.body[split.symtab] = bytes(raw)
    split.sh[split.symtab][7] = first_global
    split.body[split.sh[split.symtab][6]] = bytes(strtab)
    for sec, relas in split.relas.items():
        split.body[sec] = b"".join(struct.pack(">IIi", *r) for r in relas)
    if comment is not None:
        head = split.body[split.comment][:COMMENT_HEADER]
        split.body[split.comment] = head + b"".join(comment[i] for i in order)
    return split.write(), stats


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("split_obj", type=Path)
    parser.add_argument("src_obj", type=Path)
    parser.add_argument("-o", "--output", type=Path, required=True)
    parser.add_argument("-v", "--verbose", action="store_true")
    args = parser.parse_args()
    data, stats = build(Elf(args.split_obj.read_bytes()), Elf(args.src_obj.read_bytes()))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(data)
    if args.verbose:
        print(f"{args.output}: {stats['replaced']} replaced, {stats['promoted']} promoted, "
              f"{stats['kept_functions']} functions still from the original", file=sys.stderr)


if __name__ == "__main__":
    main()
