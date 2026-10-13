###
# Minimal reader for the retail Moh2RelGC.elf: section contents, symbols and
# relocations (which the retail ELF keeps), plus symbols.txt / splits.txt.
###

import bisect
import re
import struct
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Optional, Tuple


@dataclass
class Sym:
    name: str
    section: str
    addr: int
    size: int
    kind: str      # function, object, label
    scope: str     # global, local, weak
    data: str = ""  # dtk data hint: string, float, double, 4byte...


class Program:
    def __init__(self, root: Path, version: str = "GMFE69"):
        self.root = root
        cfg = root / "config" / version
        self.data = (root / "orig" / version / "Moh2RelGC.elf").read_bytes()
        self._read_elf()
        self.syms: List[Sym] = []
        for line in open(cfg / "symbols.txt"):
            m = re.match(r"^(\S+) = (\S+):0x(\w+); // (.*)$", line)
            if not m:
                continue
            attrs = dict(a.split(":", 1) for a in m.group(4).split() if ":" in a)
            self.syms.append(Sym(m.group(1), m.group(2), int(m.group(3), 16),
                                 int(attrs.get("size", "0"), 0),
                                 attrs.get("type", "label"), attrs.get("scope", "global"),
                                 attrs.get("data", "")))
        self.by_addr: Dict[int, List[Sym]] = defaultdict(list)
        for s in self.syms:
            self.by_addr[s.addr].append(s)
        self.by_name: Dict[str, List[Sym]] = defaultdict(list)
        for s in self.syms:
            self.by_name[s.name].append(s)
        self.units = self._read_splits(cfg / "splits.txt")

    # ------------------------------------------------------------- ELF
    def _read_elf(self) -> None:
        d = self.data
        shoff = struct.unpack(">I", d[0x20:0x24])[0]
        es, n, si = struct.unpack(">HHH", d[0x2E:0x34])
        S = [struct.unpack(">10I", d[shoff + i * es:shoff + i * es + 40]) for i in range(n)]

        def cstr(o: int) -> str:
            return d[o:d.index(b"\0", o)].decode("latin1")

        names = [cstr(S[si][4] + s[0]) for s in S]
        self.sections = [(names[i], s[3], s[5], s[4], s[1]) for i, s in enumerate(S)]
        symtab = S[names.index(".symtab")]
        strtab = S[symtab[6]]
        elf_syms = []
        for j in range(symtab[5] // 16):
            nm, v, sz, info, oth, shn = struct.unpack(
                ">IIIBBH", d[symtab[4] + j * 16:symtab[4] + j * 16 + 16])
            elf_syms.append((cstr(strtab[4] + nm), v))
        # relocation target address for every relocated word: VA -> target VA
        self.relocs: Dict[int, Tuple[int, int]] = {}
        for s in S:
            if s[1] == 4:
                for j in range(s[5] // 12):
                    off, info, add = struct.unpack(">IIi", d[s[4] + j * 12:s[4] + j * 12 + 12])
                    self.relocs[off] = (info & 0xFF, elf_syms[info >> 8][1] + add)

    def read_u32(self, addr: int) -> Optional[int]:
        for name, va, size, off, typ in self.sections:
            if typ == 1 and va and va <= addr < va + size:
                return struct.unpack(">I", self.data[off + addr - va:off + addr - va + 4])[0]
        return None

    def read_cstring(self, addr: int) -> Optional[str]:
        for name, va, size, off, typ in self.sections:
            if typ == 1 and va and va <= addr < va + size:
                o = off + addr - va
                return self.data[o:self.data.index(b"\0", o)].decode("latin1")
        return None

    def pointer_at(self, addr: int) -> Optional[int]:
        """Target of an ADDR32 relocation at `addr`, if there is one."""
        r = self.relocs.get(addr)
        return r[1] if r and r[0] == 1 else None

    def name_at(self, addr: int, prefer_function: bool = True) -> Optional[str]:
        cands = self.by_addr.get(addr, [])
        if prefer_function:
            fn = [s for s in cands if s.kind == "function"]
            if fn:
                return fn[0].name
        pool = [s for s in cands if not s.name.startswith(("@", "lbl_"))] or cands
        return pool[0].name if pool else None

    # ------------------------------------------------------------- splits
    def _read_splits(self, path: Path):
        ranges = []
        unit = None
        for line in open(path):
            m = re.match(r"^(\S+):(\s|$)", line)
            if m and m.group(1) != "Sections":
                unit = m.group(1)
                continue
            m = re.match(r"\s+(\S+)\s+start:0x(\w+) end:0x(\w+)", line)
            if m and unit:
                ranges.append((int(m.group(2), 16), int(m.group(3), 16), unit, m.group(1)))
        ranges.sort()
        self._range_starts = [r[0] for r in ranges]
        self._ranges = ranges
        return sorted({r[2] for r in ranges})

    def unit_of(self, addr: int) -> Optional[str]:
        i = bisect.bisect_right(self._range_starts, addr) - 1
        if i >= 0 and self._ranges[i][0] <= addr < self._ranges[i][1]:
            return self._ranges[i][2]
        return None
