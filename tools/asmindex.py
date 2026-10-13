###
# Index of dtk's disassembly: one parsed instruction per address, plus simple
# per-function analyses used by gen_headers.py (field accesses through `this`,
# whether `this` is used at all, rough return kind, vtable stores, sizes passed
# to operator new).
###

import re
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Optional, Set, Tuple

LINE_RE = re.compile(r"^/\* ([0-9A-F]{8}) [0-9A-F]{8}  (?:[0-9A-F]{2} ){4}\*/\t(\S+)\s*(.*)$")
MEM_RE = re.compile(r"^(-?(?:0x[0-9a-fA-F]+|\d+))\((r\d+)\)$")
GPR_RE = re.compile(r"^r(\d+)$")

# load/store mnemonic -> (width, kind)
MEMOPS = {
    "lwz": (4, "int"), "lwzu": (4, "int"), "stw": (4, "int"), "stwu": (4, "int"),
    "lhz": (2, "int"), "lha": (2, "sint"), "lhzu": (2, "int"), "sth": (2, "int"),
    "lbz": (1, "int"), "lbzu": (1, "int"), "stb": (1, "int"),
    "lfs": (4, "float"), "stfs": (4, "float"), "lfsu": (4, "float"), "stfsu": (4, "float"),
    "lfd": (8, "double"), "stfd": (8, "double"),
    "psq_l": (8, "float2"), "psq_st": (8, "float2"),
}
VOLATILE = {0, *range(3, 13)}


@dataclass
class Insn:
    addr: int
    mn: str
    ops: List[str]
    label: bool = False   # a branch target / block start


def gprs(op: str) -> List[int]:
    m = GPR_RE.match(op)
    if m:
        return [int(m.group(1))]
    m = MEM_RE.match(op)
    if m:
        return [int(m.group(2)[1:])]
    return []


def reads_writes(i: Insn) -> Tuple[Set[int], Set[int]]:
    mn, ops = i.mn.rstrip("."), i.ops
    regs = [gprs(o) for o in ops]
    flat = [r for rs in regs for r in rs]
    if mn.startswith(("st", "cmp", "mt", "dcb", "icb", "tw")) or mn in ("b", "bctr", "blr"):
        writes = set()
        if mn.endswith("u") and len(ops) > 1:      # update forms write the base
            writes |= set(gprs(ops[1]))
        return set(flat), writes
    if mn.startswith("b"):
        return set(), set()
    if not ops or not GPR_RE.match(ops[0]):
        return set(flat), set()  # FPR destination: every GPR operand is read
    dest = regs[0][0]
    reads = set(r for rs in regs[1:] for r in rs)
    if mn == "li" or mn == "lis":
        reads = set()
    writes = {dest}
    if mn.endswith("u") and mn.startswith("l") and len(ops) > 1:
        writes |= set(gprs(ops[1]))
    if mn in ("lmw",):
        writes = set(range(dest, 32))
    return reads, writes


class AsmIndex:
    def __init__(self, paths: List[Path]):
        self.insns: Dict[int, Insn] = {}
        for path in paths:
            pending_label = False
            for line in open(path, errors="replace"):
                if line.startswith((".L_", ".fn ")):
                    pending_label = True
                    continue
                m = LINE_RE.match(line.rstrip("\n"))
                if not m:
                    continue
                ops = [o.strip() for o in re.split(r",\s*", m.group(3))] if m.group(3) else []
                addr = int(m.group(1), 16)
                self.insns[addr] = Insn(addr, m.group(2), ops, pending_label)
                pending_label = False

    def function(self, addr: int, size: int) -> List[Insn]:
        return [self.insns[a] for a in range(addr, addr + size, 4) if a in self.insns]


# ------------------------------------------------------------------ analyses

def reads_before_write(body: List[Insn], reg: int) -> bool:
    """Is `reg` read (in program order) before being written or clobbered by a call?"""
    for i in body:
        if i.mn == "bl":
            return False
        if i.mn in ("b", "bctr") and reg in VOLATILE:
            # tail call: arguments pass through untouched
            return False
        r, w = reads_writes(i)
        if reg in r:
            return True
        if reg in w:
            return False
    return False


@dataclass
class Access:
    offset: int
    width: int
    kind: str
    store: bool
    pointer: bool = False


def this_accesses(body: List[Insn]):
    """Field accesses through `this` (r3 on entry), vtable stores and sub-object addresses.

    Linear scan: `this` is tracked in r3 and any register it is copied into,
    until that register is overwritten (or, for volatile registers, a call).
    """
    this: Set[int] = {3}
    vt_regs: Dict[int, str] = {}      # register -> vtable symbol (+offset) it holds
    hi: Dict[int, str] = {}           # register -> symbol from lis @ha
    loaded_from_this: Dict[int, Access] = {}
    accesses: List[Access] = []
    vptr_stores: List[Tuple[int, str]] = []
    subobjects: Set[int] = set()
    for i in body:
        mn, ops = i.mn, i.ops
        base_mem = MEM_RE.match(ops[1]) if len(ops) > 1 else None
        # pointer evidence: a value loaded from this->field used as a base or call target
        for o in ops[1:] if mn.startswith(("l", "st", "psq")) else []:
            m = MEM_RE.match(o)
            if m and int(m.group(2)[1:]) in loaded_from_this:
                loaded_from_this[int(m.group(2)[1:])].pointer = True
        if mn in ("mtctr", "mtlr") and ops and gprs(ops[0]) and gprs(ops[0])[0] in loaded_from_this:
            loaded_from_this[gprs(ops[0])[0]].pointer = True
        if base_mem and mn in MEMOPS:
            base = int(base_mem.group(2)[1:])
            if base in this:
                width, kind = MEMOPS[mn]
                acc = Access(int(base_mem.group(1), 0), width, kind, mn.startswith(("st", "psq_st")))
                accesses.append(acc)
                if acc.store and mn == "stw" and gprs(ops[0]) and gprs(ops[0])[0] in vt_regs:
                    vptr_stores.append((acc.offset, vt_regs[gprs(ops[0])[0]]))
                if not acc.store and kind == "int" and width == 4 and gprs(ops[0]):
                    loaded_from_this[gprs(ops[0])[0]] = acc
        # track registers holding vtable addresses
        if mn == "lis" and len(ops) == 2 and ops[1].endswith("@ha"):
            hi[gprs(ops[0])[0]] = ops[1][:-3]
        set_vt = None
        if mn == "addi" and len(ops) == 3 and ops[2].endswith("@l") and ops[2][:-2].startswith("__vt__"):
            set_vt = gprs(ops[0])[0]
        elif mn == "addi" and len(ops) == 3 and gprs(ops[1]) and gprs(ops[1])[0] in this:
            try:
                subobjects.add(int(ops[2], 0))
            except ValueError:
                pass
        # update tracking
        r, w = reads_writes(i)
        if mn == "mr" and len(ops) == 2 and gprs(ops[1])[0] in this:
            this.add(gprs(ops[0])[0])
            w = set()
        copied_vt = vt_regs.get(gprs(ops[1])[0]) if mn == "mr" and len(ops) == 2 else None
        for reg in w:
            this.discard(reg)
            vt_regs.pop(reg, None)
            loaded_from_this.pop(reg, None)
        if set_vt is not None:
            vt_regs[set_vt] = ops[2][:-2]
        if copied_vt is not None:
            vt_regs[gprs(ops[0])[0]] = copied_vt
        if mn == "bl":
            this -= VOLATILE
            for reg in list(vt_regs):
                if reg in VOLATILE:
                    vt_regs.pop(reg)
            for reg in list(loaded_from_this):
                if reg in VOLATILE:
                    loaded_from_this.pop(reg)
    return accesses, vptr_stores, subobjects


def return_kind(body: List[Insn]) -> str:
    """'void', 'bool', 'int' or 'float' from what reaches r3/f1 before each blr.

    Walks back from each blr through its basic block. A write to r3 means an
    integer-like return; a write to f1 that is not consumed afterwards means a
    float return. Only both 0 and 1 being returned counts as bool.
    """
    kinds = set()
    consts = set()
    for idx, i in enumerate(body):
        if i.mn != "blr":
            continue
        f1_used = False
        j = idx - 1
        while j >= 0:
            p = body[j]
            if p.mn == "bl" or (p.mn.startswith("b") and p.mn != "blr"):
                break
            r, w = reads_writes(p)
            if 3 in w:
                if p.mn == "li" and p.ops[1] in ("0x0", "0x1", "0", "1"):
                    consts.add(int(p.ops[1], 0))
                    kinds.add("const")
                else:
                    kinds.add("int")
                break
            fregs = [o for o in p.ops if re.fullmatch(r"f\d+", o)]
            if "f1" in fregs:
                is_store = p.mn.startswith(("st", "psq_st", "fcmp"))
                if not is_store and fregs[0] == "f1" and not f1_used:
                    kinds.add("float")
                    break
                f1_used = True
            if p.label:
                break
            j -= 1
    if "int" in kinds:
        return "int"
    if "const" in kinds:
        return "bool" if consts == {0, 1} else "int"
    if "float" in kinds:
        return "float"
    return "void"


# allocation functions and the register holding the requested size
ALLOCATORS = (("__nw__", "r3"), ("DWI_alloc__", "r4"))


def new_sizes(index: AsmIndex) -> Dict[str, Set[int]]:
    """Constructor -> sizes passed to an allocator right before it runs."""
    out: Dict[str, Set[int]] = defaultdict(set)
    addrs = sorted(index.insns)
    pos = {a: k for k, a in enumerate(addrs)}
    for a in addrs:
        i = index.insns[a]
        if i.mn != "bl" or not i.ops:
            continue
        reg = next((r for prefix, r in ALLOCATORS if i.ops[0].startswith(prefix)), None)
        if reg is None:
            continue
        k = pos[a]
        size = None
        for back in range(1, 8):
            if k - back < 0:
                break
            p = index.insns[addrs[k - back]]
            if p.mn == "li" and p.ops[0] == reg:
                size = int(p.ops[1], 0)
                break
            if p.mn == "bl":
                break
        if size is None:
            continue
        for fwd in range(1, 24):
            if k + fwd >= len(addrs):
                break
            p = index.insns[addrs[k + fwd]]
            if p.mn == "bl":
                if p.ops and p.ops[0].startswith("__ct__"):
                    out[p.ops[0]].add(size)
                break
    return out
