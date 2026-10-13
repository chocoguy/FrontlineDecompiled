#!/usr/bin/env python3

###
# Draft C/C++ for a unit or a function with m2c.
#
# Runs m2c on dtk's disassembly with a context generated from the class model
# (tools/m2c_context.py), then rewrites m2c's mangled C into C++ syntax:
#   int GetSceneNode__9CAIDoodadCFv(CAIDoodad *this)  ->  int CAIDoodad::GetSceneNode() const
#   Update__13CFaderControlFf(fader, dt)               ->  fader->Update(dt)
#
# Drafts are a starting point: they need cleanup before going into src/.
#
#   python tools/draft.py game/MallocInit.cpp            # whole unit -> drafts/game/MallocInit.cpp
#   python tools/draft.py -f GetSceneNode__9CAIDoodadCFv  # one function -> stdout
#   python tools/draft.py -f CAIDoodad::Init             # demangled names work too
#   python tools/draft.py game/AIDoodad.cpp --raw        # keep m2c's mangled C
#   python tools/draft.py ... -- --no-andor              # extra args go to m2c
#
# m2c is looked for in ../m2c-master/m2c.py (override with --m2c or $M2C).
###

import argparse
import os
import re
import subprocess
import sys
from pathlib import Path
from typing import Dict, List, Optional, Tuple

sys.path.insert(0, str(Path(__file__).resolve().parent))
from cwdemangle import demangle, render  # noqa: E402
from gen_headers import ROOT, VERSION, Model, header_path  # noqa: E402

CONTEXT = ROOT / "build" / "m2c" / "context.c"
DRAFTS = ROOT / "drafts"
CONTEXT_INPUTS = [
    ROOT / "config" / VERSION / "symbols.txt",
    ROOT / "config" / VERSION / "splits.txt",
    *(ROOT / "tools").glob("*.py"),
]


def find_m2c(arg: Optional[Path]) -> Path:
    for cand in [arg, os.environ.get("M2C"), ROOT.parent / "m2c-master" / "m2c.py",
                 ROOT.parent / "m2c" / "m2c.py"]:
        if cand and Path(cand).is_file():
            return Path(cand)
    sys.exit("m2c.py not found: pass --m2c or set $M2C")


def ensure_context() -> None:
    newest = max(p.stat().st_mtime for p in CONTEXT_INPUTS if p.exists())
    if CONTEXT.exists() and CONTEXT.stat().st_mtime >= newest:
        return
    print("regenerating m2c context...", file=sys.stderr)
    subprocess.run([sys.executable, str(ROOT / "tools" / "m2c_context.py"), "-o", str(CONTEXT)],
                   check=True, stdout=subprocess.DEVNULL)


def resolve_function(m: Model, name: str) -> Tuple[str, str]:
    """(mangled name, unit) for a mangled, plain or demangled function name."""
    cands = [s for s in m.prog.syms if s.kind == "function"]
    exact = [s for s in cands if s.name == name]
    if not exact:
        exact = []
        for s in cands:
            d = demangle(s.name)
            if d and d.is_function and "::".join([*d.scope, d.name]) == name:
                exact.append(s)
    if not exact:
        sys.exit(f"no function named {name!r}")
    if len(exact) > 1:
        print(f"{name!r} is ambiguous, using the first of:", file=sys.stderr)
        for s in exact:
            print(f"    {s.name}  0x{s.addr:08X}", file=sys.stderr)
    s = exact[0]
    unit = m.prog.unit_of(s.addr)
    if unit is None:
        sys.exit(f"{s.name} is not in any unit")
    return s.name, unit


# ------------------------------------------------------------------ C++ rewrite

IDENT = re.compile(r"\b([A-Za-z_][A-Za-z0-9_]*__[A-Za-z0-9_<>,]*)\(")


def split_args(text: str, start: int) -> Tuple[List[str], int]:
    """Split the argument list starting after '(' at `start`; return (args, end index after ')')."""
    depth, cur, args, i = 0, "", [], start
    while i < len(text):
        ch = text[i]
        if ch in "([{":
            depth += 1
        elif ch in ")]}":
            if depth == 0:
                if cur.strip() or args:
                    args.append(cur.strip())
                return args, i + 1
            depth -= 1
        if ch == "," and depth == 0:
            args.append(cur.strip())
            cur = ""
        else:
            cur += ch
        i += 1
    raise ValueError("unbalanced")


def member_expr(obj: str) -> str:
    if obj == "this":
        return "this->"
    if obj.startswith("&") and re.fullmatch(r"&[\w.\->\[\]]+", obj):
        return obj[1:] + "."
    if re.fullmatch(r"[\w.\->\[\]]+", obj):
        return obj + "->"
    return f"({obj})->"


class Rewriter:
    def __init__(self, m: Model):
        self.static = set()
        for info in m.classes.values():
            for f in info.methods:
                if f.static:
                    self.static.add(f.sym.mangled)

    def call(self, mangled: str, args: List[str]) -> Optional[str]:
        d = demangle(mangled)
        if d is None or not d.is_function:
            return None
        if d.special in ("ct", "dt") or "<" in d.name:
            return None
        if not d.scope:
            return f"{d.name}({', '.join(args)})"
        if mangled in self.static or d.name.startswith("operator new"):
            return f"{'::'.join(d.scope)}::{d.name}({', '.join(args)})"
        if not args:
            return None
        return f"{member_expr(args[0])}{d.name}({', '.join(args[1:])})"

    def calls(self, line: str) -> str:
        out, pos = "", 0
        while True:
            m = IDENT.search(line, pos)
            if not m:
                return out + line[pos:]
            try:
                args, end = split_args(line, m.end())
            except ValueError:
                return out + line[pos:]
            args = [self.calls(a) for a in args]
            new = self.call(m.group(1), args)
            if new is None:
                out += line[pos:m.end()] + ", ".join(args) + ")"
            else:
                out += line[pos:m.start()] + new
            pos = end

    def definition(self, line: str) -> Optional[str]:
        """Rewrite `RET Name__mangled(Class *this, T arg0) {` into C++."""
        m = re.match(r"^(static )?(.*?)\b([A-Za-z_]\w*__[\w<>,]*)\((.*)\) \{$", line)
        if not m:
            return None
        d = demangle(m.group(3))
        if d is None or not d.is_function:
            return None
        names = [p.strip().rsplit(" ", 1)[-1].lstrip("*") for p in m.group(4).split(",") if p.strip()]
        if d.scope and m.group(3) not in self.static:
            names = names[1:]                                  # this
        if d.special == "dt" and names:
            names = names[1:]                                  # destroyFlag
        params = []
        for k, t in enumerate(d.params):
            name = names[k] if k < len(names) and names[k] not in ("void", "...") else f"arg{k}"
            if t.kind == "builtin" and t.name == "...":
                params.append("...")
            else:
                from cwdemangle import declarator
                params.append(declarator(t, name))
        q = "::".join([*d.scope, d.name])
        tail = " const" if d.const else ""
        ret = "" if d.special in ("ct", "dt") else m.group(2).strip().replace("?", "void") + " "
        return f"{ret}{q}({', '.join(params)}){tail} {{  // {m.group(3)}"

    def prototype(self, line: str) -> Optional[str]:
        """m2c's prototypes for mangled functions: the included header declares
        them in C++, so keep them only as a comment."""
        m = re.match(r"^(.*?)\b([A-Za-z_]\w*__[\w<>,]*)\(.*\);\s*/\* (extern|static) \*/\s*$", line)
        if not m:
            return None
        d = demangle(m.group(2))
        if d is None or not d.is_function:
            return None
        from cwdemangle import full_signature
        return f"// {m.group(3)}: {full_signature(d)}"

    def rewrite(self, text: str) -> str:
        out = []
        for line in text.splitlines():
            new = self.prototype(line)
            if new is None:
                new = self.definition(line)
            out.append(new if new is not None else self.calls(line))
        return "\n".join(out) + "\n"


CRBITS = {"lt": 0, "gt": 1, "eq": 2, "so": 3, "un": 3}


def crbit(op: str) -> int:
    """dtk's symbolic condition bits (eq, cr1eq, 4*cr1+eq) as numbers, as m2c expects."""
    m = re.fullmatch(r"(?:4\*cr(\d)\+|cr(\d))?(lt|gt|eq|so|un)", op)
    if not m:
        return int(op, 0)
    field = int(m.group(1) or m.group(2) or 0)
    return 4 * field + CRBITS[m.group(3)]


def mfcr_extract(m: "re.Match") -> str:
    """`extrwi rA, rS, 1, b` / `srwi rA, rS, 31` after mfcr -> the rlwinm m2c matches."""
    mn, ra, rs, a, b = m.group(3), m.group(4), m.group(5), int(m.group(6)), m.group(7)
    if mn.startswith("extrwi") and a == 1 and b is not None:
        shift = int(b) + 1
    elif mn == "srwi" and a == 31:
        shift = 1
    else:
        return m.group(0)
    dot = "." if mn.endswith(".") else ""
    return f"{m.group(1)}rlwinm{dot} {ra}, {rs}, {shift}, 31, 31"


def reverse_functions(text: str) -> str:
    """List function definitions in reverse address order.

    The game was compiled with deferred inlining, which emits functions in the
    reverse of their source order, so this is the original source order.
    """
    lines = text.split("\n")
    blocks, i, first = [], 0, None
    while i < len(lines):
        if re.match(r"^[^\s/#].*\)( const)? \{(\s*//.*)?$", lines[i]):
            start = i - 1 if i > 0 and lines[i - 1].startswith("/*") else i
            end = i
            while end < len(lines) and lines[end] != "}":
                end += 1
            if first is None:
                first = start
            blocks.append("\n".join(lines[start:end + 1]))
            i = end + 1
            while i < len(lines) and not lines[i].strip():
                i += 1
            continue
        i += 1
    if first is None or len(blocks) < 2:
        return text
    return "\n".join(lines[:first]) + "\n" + "\n\n".join(reversed(blocks)) + "\n"


def prepare_asm(asm: Path) -> Path:
    """Copy of `asm` with jump tables renamed so m2c recognizes them.

    m2c only treats tables named jtbl*/jumptable_*/... as jump tables, but this
    project keeps the compiler's real local names (e.g. "@624"). A table is an
    object made only of `.rel` entries.
    """
    text = asm.read_text(errors="replace")
    renames = {}
    for m in re.finditer(r'^\.obj ("?[^",\n]+"?), local\n((?:\t\.rel [^\n]+\n)+)\.endobj', text, re.M):
        old = m.group(1)
        renames[old] = "jumptable_" + re.sub(r"\W", "", old.strip('"'))
    for old, new in renames.items():
        text = re.sub(rf"(?<![\w@]){re.escape(old)}(?![\w])", new, text)
    text = re.sub(r"\tcror ([\w*+]+), ([\w*+]+), ([\w*+]+)$",
                  lambda m: "\tcror " + ", ".join(str(crbit(x)) for x in m.groups()), text, flags=re.M)
    text = re.sub(r"(\tmfcr (r\d+)\n[^\n]*\t)(extrwi\.?|srwi) (r\d+), (r\d+), (\d+)(?:, (\d+))?$",
                  mfcr_extract, text, flags=re.M)
    out = ROOT / "build" / "m2c" / "asm" / asm.relative_to(ROOT / "build" / VERSION / "asm")
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(text)
    return out


# ------------------------------------------------------------------ data

def c_string(prog, addr: int) -> Optional[str]:
    """A C string literal if `addr` points at printable NUL-terminated text."""
    text = prog.read_cstring(addr)
    if text is None or len(text) > 200:
        return None
    if not all(32 <= ord(c) < 127 or c in "\n\t\r" for c in text):
        return None
    return '"' + text.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n") \
        .replace("\t", "\\t").replace("\r", "\\r") + '"'


def target_expr(prog, addr: int) -> str:
    s = c_string(prog, addr)
    if s:
        return f"(u32){s}"
    name = prog.name_at(addr, prefer_function=True)
    if name:
        return f"(u32)&{name}"
    # nearest preceding symbol + offset
    best = max((a for a in prog.by_addr if a <= addr), default=None)
    if best is not None:
        return f"(u32)&{prog.by_addr[best][0].name} + 0x{addr - best:X}"
    return f"0x{addr:08X}"


def raw_initializer(prog, unit: str, name: str, qualifiers: str) -> Optional[str]:
    name = name.split("[", 1)[0]
    syms = [s for s in prog.by_name.get(name, []) if prog.unit_of(s.addr) == unit
            and not s.section.startswith(("extab", "extabindex"))]
    if not syms or not syms[0].size:
        if any(prog.unit_of(s.addr) == unit for s in prog.by_name.get(name, [])):
            return f"// {name}: exception table, generated by the compiler"
        return None
    s = syms[0]
    if name.startswith("@"):
        # compiler literal pool entry: in source this is just a literal
        if s.data == "string":
            return f"// {name}: string literal {c_string(prog, s.addr) or '?'}"
        if s.data in ("float", "double") or s.section.startswith(".sdata2"):
            import struct
            if s.size == 4:
                v = struct.unpack(">f", (prog.read_u32(s.addr) or 0).to_bytes(4, "big"))[0]
                return f"// {name}: float literal {v!r}"
            if s.size == 8:
                raw = ((prog.read_u32(s.addr) or 0) << 32) | (prog.read_u32(s.addr + 4) or 0)
                v = struct.unpack(">d", raw.to_bytes(8, "big"))[0]
                return f"// {name}: double literal {v!r}"
        text = c_string(prog, s.addr)
        if text is not None or prog.read_u32(s.addr & ~3) is not None and s.size <= 4:
            return f"// {name}: compiler literal {text or '(' + str(s.size) + ' bytes)'}"
    if s.section.startswith((".bss", ".sbss")):
        return f"{qualifiers}u8 {name}[0x{s.size:X}];"
    const = s.section.startswith((".rodata", ".sdata2"))
    # a const global needs `extern` in C++, or it gets internal linkage
    qualifiers = ("static " if s.scope == "local" else ("extern " if const else "")) + \
        ("const " if const else "")
    words = s.size % 4 == 0 and s.addr % 4 == 0
    has_reloc = any(prog.pointer_at(a) is not None for a in range(s.addr, s.addr + s.size, 4))
    if words and (has_reloc or s.size >= 4):
        vals = []
        for a in range(s.addr, s.addr + s.size, 4):
            t = prog.pointer_at(a)
            vals.append(target_expr(prog, t) if t is not None else f"0x{prog.read_u32(a):08X}")
        ctype, items = "u32", vals
        if all(v.startswith('(u32)"') for v in vals):
            ctype, items = "char*", [v[5:] for v in vals]       # table of strings
        elif all(v == "0x00000000" for v in vals):
            return f"{qualifiers}int {name}[{len(vals)}];  // zero-initialized, at 0x{s.addr:08X}"
    else:
        data = b"".join((prog.read_u32(a & ~3) or 0).to_bytes(4, "big") for a in range(s.addr & ~3, s.addr + s.size, 4))
        off = s.addr & 3
        ctype, items = "u8", [f"0x{b:02X}" for b in data[off:off + s.size]]
    per = 1 if ctype == "char*" else (4 if ctype == "u32" and has_reloc else (8 if ctype == "u32" else 16))
    rows = [", ".join(items[i:i + per]) for i in range(0, len(items), per)]
    body = ",\n    ".join(rows)
    return (f"{qualifiers}{ctype} {name}[{len(items)}] = {{  // at 0x{s.addr:08X}\n"
            f"    {body}\n}};")


def fill_data(prog, unit: str, text: str) -> str:
    out = []
    for line in text.splitlines():
        m = re.match(r"^(static )?(.*?)(\S+);\s*/\* (?:[^*]*; )?unable to generate initializer: ([^*]*)\*/", line)
        if m and (m.group(3).startswith(("__RTTI__", "__vt__"))):
            out.append(f"// {m.group(3)}: generated by the compiler")
            continue
        if m:
            quals = (m.group(1) or "") + ("const " if "const" in m.group(4) else "")
            raw = raw_initializer(prog, unit, m.group(3), quals)
            if raw and raw.startswith("//"):
                out.append(raw)
                continue
            if raw:
                out.append(f"// m2c: {line.strip()}")
                out.append(raw)
                continue
        if re.match(r"^static \? gap_\w+;", line):
            out.append(f"// {line.strip()}  (alignment padding)")
            continue
        out.append(line)
    return "\n".join(out) + "\n"


# ------------------------------------------------------------------ main

def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("unit", nargs="?", help="unit path, e.g. game/MallocInit.cpp")
    parser.add_argument("-f", "--function", action="append", default=[],
                        help="function to draft (mangled, plain or Class::method); repeatable")
    parser.add_argument("-o", "--output", type=Path, help="output file (default: drafts/<unit> or stdout for -f)")
    parser.add_argument("--stdout", action="store_true")
    parser.add_argument("--raw", action="store_true", help="keep m2c's mangled C output")
    parser.add_argument("--m2c", type=Path)
    argv = sys.argv[1:]
    extra = argv[argv.index("--") + 1:] if "--" in argv else []
    args = parser.parse_args(argv[:argv.index("--")] if "--" in argv else argv)

    m2c = find_m2c(args.m2c)
    m = Model()
    funcs = []
    unit = args.unit
    for name in args.function:
        mangled, u = resolve_function(m, name)
        if unit and u != unit:
            sys.exit(f"{name} is in {u}, not {unit}")
        unit = u
        funcs.append(mangled)
    if not unit:
        parser.error("give a unit or -f FUNCTION")
    asm = ROOT / "build" / VERSION / "asm" / (re.sub(r"\.[A-Za-z]+$", "", unit) + ".s")
    if not asm.exists():
        sys.exit(f"{asm} not found (run ninja first)")

    ensure_context()
    lang = "c" if unit.endswith(".c") else "c++"
    if not funcs and ".fn " not in asm.read_text(errors="replace") and \
            not any(a.startswith("--globals") for a in extra):
        extra.append("--globals=all")   # data-only unit
    cmd = [sys.executable, str(m2c), "-t", f"ppc-mwcc-{lang}", "--context", str(CONTEXT),
           "--pointer-style", "left", *extra]
    for f in funcs:
        cmd += ["-f", f]
    cmd.append(str(prepare_asm(asm)))
    r = subprocess.run(cmd, capture_output=True, text=True)
    # m2c exits with 1 when some functions failed but still prints the rest
    if not r.stdout.strip() and ".fn " not in asm.read_text(errors="replace"):
        # data-only unit m2c had nothing to say about (e.g. only .sbss)
        objs = sorted((s for s in m.prog.syms if s.kind == "object" and m.prog.unit_of(s.addr) == unit
                       and not s.name.startswith("gap_")), key=lambda s: s.addr)
        r = subprocess.CompletedProcess(cmd, 0, "\n".join(
            f"static ? {s.name}; /* unable to generate initializer: unknown type */" for s in objs) + "\n", "")
    if not r.stdout.strip():
        sys.stderr.write(r.stderr)
        sys.exit(r.returncode or 1)
    text = r.stdout
    failed = re.findall(r"Decompilation failure in function (\S+?):", text)
    for f in failed:
        print(f"warning: m2c could not decompile {f} (see comment in the draft)", file=sys.stderr)
    if not args.raw and lang == "c++":
        text = Rewriter(m).rewrite(text)
    if not args.raw:
        text = fill_data(m.prog, unit, text)
        if not funcs:
            text = reverse_functions(text)

    header = header_path(unit)
    banner = (f"// m2c draft of {unit}{' (' + ', '.join(funcs) + ')' if funcs else ''}\n"
              f"// Not compiled: clean up, then move into src/{unit}.\n"
              f"// Functions are in reverse address order (= original source order).\n")
    if (ROOT / "include" / header).exists():
        banner += f'#include "{header}"\n'
    text = banner + "\n" + text

    to_stdout = args.stdout or (funcs and not args.output)
    if to_stdout:
        sys.stdout.write(text)
        return
    out = args.output or DRAFTS / unit
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(text)
    print(f"wrote {out}")


if __name__ == "__main__":
    main()
