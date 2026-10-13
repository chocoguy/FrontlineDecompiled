###
# Structured CodeWarrior C++ symbol demangler.
#
# Unlike `dtk demangle`, which only prints a string, this returns the pieces
# (scope, name, parameter types, const-ness) so tools can generate headers.
# Output strings follow the same formatting as `dtk demangle` so the two can
# be compared (see `python tools/cwdemangle.py --check`).
###

import re
from dataclasses import dataclass, field
from typing import List, Optional, Tuple

BUILTINS = {
    "v": "void", "b": "bool", "c": "char", "s": "short", "i": "int",
    "l": "long", "x": "long long", "f": "float", "d": "double",
    "r": "long double", "w": "wchar_t", "e": "...",
}

OPERATORS = {
    "nw": "operator new", "nwa": "operator new[]", "dl": "operator delete",
    "dla": "operator delete[]", "pl": "operator+", "mi": "operator-",
    "ml": "operator*", "dv": "operator/", "md": "operator%", "er": "operator^",
    "ad": "operator&", "or": "operator|", "co": "operator~", "nt": "operator!",
    "as": "operator=", "lt": "operator<", "gt": "operator>", "apl": "operator+=",
    "ami": "operator-=", "amu": "operator*=", "adv": "operator/=",
    "amd": "operator%=", "aer": "operator^=", "aad": "operator&=",
    "aor": "operator|=", "ls": "operator<<", "rs": "operator>>",
    "ars": "operator>>=", "als": "operator<<=", "eq": "operator==",
    "ne": "operator!=", "le": "operator<=", "ge": "operator>=",
    "aa": "operator&&", "oo": "operator||", "pp": "operator++",
    "mm": "operator--", "cm": "operator,", "rm": "operator->*",
    "rf": "operator->", "cl": "operator()", "vc": "operator[]",
}


class DemangleError(Exception):
    pass


@dataclass
class Type:
    """A parsed type. `kind` is one of builtin, class, ptr, ref, array, func, memptr."""
    kind: str
    name: str = ""                     # builtin spelling or class display name
    scope: Tuple[str, ...] = ()        # class path, e.g. ("RCMP", "AV_PLAYER")
    inner: Optional["Type"] = None     # pointee / element / return type
    params: List["Type"] = field(default_factory=list)
    cls: Optional["Type"] = None       # class of a pointer to member
    const: bool = False
    volatile: bool = False
    unsigned: bool = False
    signed: bool = False
    count: int = 0                     # array length
    func_const: bool = False           # const member function pointer

    def classes(self):
        """Yield (class Type, by_value) for every class this type mentions."""
        if self.kind == "class":
            yield self, True
            for a in template_args(self):
                if isinstance(a, Type):
                    for c, _ in a.classes():
                        yield c, False
        elif self.kind in ("ptr", "ref", "array"):
            for c, _ in self.inner.classes():
                yield c, False
        elif self.kind == "func":
            for t in [self.inner, *self.params]:
                yield from t.classes()
        elif self.kind == "memptr":
            for c, _ in self.cls.classes():
                yield c, False
            for c, _ in self.inner.classes():
                yield c, False


def template_args(t: Type):
    return getattr(t, "_targs", [])


@dataclass
class Symbol:
    mangled: str
    name: str                          # unqualified member/function name (display)
    scope: Tuple[str, ...] = ()        # enclosing classes/namespaces (display names)
    scope_types: List[Type] = field(default_factory=list)
    params: Optional[List[Type]] = None  # None for data symbols
    const: bool = False
    special: str = ""                  # ct, dt, op, conv, vt, rtti, or ""
    conv_type: Optional[Type] = None   # conversion operator target type

    @property
    def is_function(self) -> bool:
        return self.params is not None


class _Parser:
    def __init__(self, s: str):
        self.s = s
        self.i = 0

    def peek(self, n: int = 1) -> str:
        return self.s[self.i:self.i + n]

    def eof(self) -> bool:
        return self.i >= len(self.s)

    def take(self, n: int = 1) -> str:
        if self.i + n > len(self.s):
            raise DemangleError("unexpected end")
        r = self.s[self.i:self.i + n]
        self.i += n
        return r

    def number(self) -> int:
        m = re.match(r"\d+", self.s[self.i:])
        if not m:
            raise DemangleError(f"expected number at {self.s[self.i:]!r}")
        self.i += len(m.group(0))
        return int(m.group(0))

    def name_component(self) -> Type:
        n = self.number()
        raw = self.take(n)
        if len(raw) != n:
            raise DemangleError("short name")
        return parse_class_name(raw)

    def qualified(self) -> Tuple[List[Type], ...]:
        """Q<n><name>... or <len><name>; returns the list of components."""
        if self.peek() == "Q":
            self.take()
            count = int(self.take())
            return [self.name_component() for _ in range(count)]
        return [self.name_component()]

    def type(self) -> Type:
        mods = {"const": False, "volatile": False, "unsigned": False, "signed": False}
        while self.peek() in ("C", "V", "U", "S"):
            c = self.take()
            mods[{"C": "const", "V": "volatile", "U": "unsigned", "S": "signed"}[c]] = True
        c = self.peek()
        if c == "P":
            self.take()
            t = Type("ptr", inner=self.type())
        elif c == "R":
            self.take()
            t = Type("ref", inner=self.type())
        elif c == "A":
            self.take()
            n = self.number()
            if self.take() != "_":
                raise DemangleError("bad array")
            t = Type("array", count=n, inner=self.type())
        elif c == "F":
            self.take()
            params = self.params(stop="_")
            self.take()  # _
            t = Type("func", params=params, inner=self.type())
        elif c == "M":
            self.take()
            comps = self.qualified()
            cls = comps[-1] if len(comps) == 1 else _scoped(comps)
            func_const = False
            if self.peek() == "C" and self.s[self.i + 1:self.i + 2] == "F":
                self.take()
                func_const = True
            t = Type("memptr", cls=cls, inner=self.type(), func_const=func_const)
        elif c == "Q" or c.isdigit():
            comps = self.qualified()
            t = comps[0] if len(comps) == 1 else _scoped(comps)
        elif c in BUILTINS:
            self.take()
            t = Type("builtin", name=BUILTINS[c])
        else:
            raise DemangleError(f"unknown type code {c!r}")
        if any(mods.values()):
            t = _copy(t)
            t.const |= mods["const"]
            t.volatile |= mods["volatile"]
            t.unsigned |= mods["unsigned"]
            t.signed |= mods["signed"]
        return t

    def params(self, stop: Optional[str] = None) -> List[Type]:
        out = []
        while not self.eof() and (stop is None or self.peek() != stop):
            out.append(self.type())
        if out and len(out) == 1 and out[0].kind == "builtin" and out[0].name == "void" \
                and not out[0].const:
            return []
        return out


def _copy(t: Type) -> Type:
    n = Type(**{k: getattr(t, k) for k in Type.__dataclass_fields__})
    if hasattr(t, "_targs"):
        n._targs = t._targs
    return n


def _scoped(comps: List[Type]) -> Type:
    last = _copy(comps[-1])
    last.scope = tuple(render(c) for c in comps[:-1]) + last.scope
    last._outer = comps[:-1]
    return last


def parse_class_name(raw: str) -> Type:
    """`Foo` or a template `Foo<args>` as written inside a length-prefixed name."""
    t = Type("class", name=raw)
    if "<" in raw and raw.endswith(">"):
        base = raw[:raw.index("<")]
        inner = raw[raw.index("<") + 1:-1]
        args = []
        for part in _split_template(inner):
            if re.fullmatch(r"-?\d+", part):
                args.append(part)
            else:
                p = _Parser(part)
                ty = p.type()
                if not p.eof():
                    raise DemangleError(f"bad template arg {part!r}")
                args.append(ty)
        t.name = base
        t._targs = args
    return t


def _split_template(s: str) -> List[str]:
    out, depth, cur = [], 0, ""
    for ch in s:
        if ch == "<":
            depth += 1
        elif ch == ">":
            depth -= 1
        if ch == "," and depth == 0:
            out.append(cur)
            cur = ""
        else:
            cur += ch
    out.append(cur)
    return out


# ---------------------------------------------------------------- rendering

def class_display(t: Type) -> str:
    """Class name including template arguments, without scope."""
    targs = template_args(t)
    if not targs:
        return t.name
    return t.name + "<" + ", ".join(a if isinstance(a, str) else render(a) for a in targs) + ">"


def qualified_name(t: Type) -> str:
    return "::".join([*t.scope, class_display(t)])


def _base(t: Type) -> str:
    if t.kind == "builtin":
        s = t.name
        if t.unsigned:
            s = "unsigned " + s
        elif t.signed:
            s = "signed " + s
    else:
        s = qualified_name(t)
    if t.volatile:
        s = "volatile " + s
    if t.const:
        s = "const " + s
    return s


def declarator(t: Type, inner: str = "") -> str:
    """Render type `t` around declarator `inner` (a name, or "" for abstract)."""
    if t.kind in ("builtin", "class"):
        if not inner:
            return _base(t)
        tight = inner[0] in "*&[" or (inner[0] == "(" and inner.endswith("]"))
        return _base(t) + (inner if tight else " " + inner)
    cv = (" const" if t.const else "") + (" volatile" if t.volatile else "")
    if t.kind in ("ptr", "ref", "memptr"):
        sym = {"ptr": "*", "ref": "&", "memptr": ""}[t.kind]
        if t.kind == "memptr":
            sym = qualified_name(t.cls) + "::*"
        d = sym + cv + (" " + inner if inner and cv else inner)
        if t.inner.kind in ("func", "array"):
            d = "(" + d + ")"
        return declarator(t.inner, d)
    if t.kind == "array":
        return declarator(t.inner, f"{inner}[{t.count}]")
    if t.kind == "func":
        params = ", ".join(render(p) for p in t.params)
        tail = " const" if t.func_const else ""
        ret = declarator(t.inner, "")
        return f"{ret} {inner}({params}){tail}" if inner else f"{ret} ({params}){tail}"
    raise DemangleError(t.kind)


def render(t: Type) -> str:
    if t.kind == "memptr" and t.inner.kind == "func":
        f = t.inner
        params = ", ".join(render(p) for p in f.params)
        tail = " const" if t.func_const else ""
        return f"{declarator(f.inner, '')} ({qualified_name(t.cls)}::*)({params}){tail}"
    return declarator(t)


def param_list(params: List[Type]) -> str:
    return ", ".join(render(p) for p in params)


def full_signature(sym: Symbol) -> str:
    q = "::".join([*sym.scope, sym.name])
    if not sym.is_function:
        return q
    return f"{q}({param_list(sym.params)})" + (" const" if sym.const else "")


# ---------------------------------------------------------------- symbols

def _split_points(s: str) -> List[int]:
    """Candidate indices of the `__` separating name from signature."""
    pts, depth = [], 0
    for i in range(len(s) - 1):
        ch = s[i]
        if ch == "<":
            depth += 1
        elif ch == ">":
            depth -= 1
        elif depth == 0 and i > 0 and s[i:i + 2] == "__":
            pts.append(i)
    return pts


def demangle(mangled: str) -> Optional[Symbol]:
    """Return a Symbol, or None if `mangled` is not a mangled C++ name."""
    s = mangled
    special_prefix = s.startswith("__")
    body = s[2:] if special_prefix else s
    for idx in _split_points(body):
        name, rest = body[:idx], body[idx + 2:]
        if not name or not rest:
            continue
        try:
            sym = _parse_rest(name, rest, special_prefix, mangled)
        except DemangleError:
            continue
        if sym:
            return sym
    return None


def _parse_rest(name: str, rest: str, special: bool, mangled: str) -> Optional[Symbol]:
    p = _Parser(rest)
    scope_types: List[Type] = []
    if p.peek() == "Q" or p.peek().isdigit():
        scope_types = p.qualified()
    cnst = False
    if p.peek() == "C" and rest[p.i + 1:p.i + 2] == "F":
        p.take()
        cnst = True
    params = None
    if p.peek() == "F":
        p.take()
        params = p.params()
    if not p.eof():
        return None
    if params is None and not scope_types:
        return None
    scope = tuple(qualified_name(c) if c.scope else class_display(c) for c in scope_types)
    if "<" in name and name.endswith(">") and not special:
        name = class_display(parse_class_name(name))
    sym = Symbol(mangled=mangled, name=name, scope=scope, scope_types=scope_types,
                 params=params, const=cnst)
    if special:
        tsuffix = ""
        if "<" in name and name.endswith(">"):
            tsuffix = class_display(parse_class_name(name))[name.index("<"):]
            name = name[:name.index("<")]
        if name in ("ct", "dt"):
            if not scope_types:
                return None
            sym.special = name
            sym.name = ("~" if name == "dt" else "") + scope_types[-1].name + tsuffix
        elif name == "vt":
            sym.special, sym.name = "vt", "__vtable"
        elif name == "RTTI":
            sym.special, sym.name = "rtti", "__RTTI"
        elif name in OPERATORS:
            sym.special, sym.name = "op", OPERATORS[name] + tsuffix
        elif name.startswith("op"):
            t = _Parser(name[2:])
            try:
                conv = t.type()
            except DemangleError:
                return None
            if not t.eof():
                return None
            sym.special, sym.name, sym.conv_type = "conv", "operator " + render(conv), conv
        else:
            sym.name = "__" + name + tsuffix
    return sym


# ---------------------------------------------------------------- self-check

def _check() -> None:
    """Compare against `dtk demangle` for every mangled name in symbols.txt."""
    import subprocess
    from concurrent.futures import ThreadPoolExecutor
    from pathlib import Path
    root = Path(__file__).resolve().parent.parent
    names = []
    for line in open(root / "config/GMFE69/symbols.txt"):
        m = re.match(r"^(\S+) = ", line)
        if m and "__" in m.group(1).lstrip("_") and not m.group(1).startswith("@"):
            names.append(m.group(1))
    names = sorted(set(names))

    def ref(n: str) -> Optional[str]:
        r = subprocess.run([str(root / "build/tools/dtk"), "demangle", n],
                           capture_output=True, text=True)
        out = r.stdout.strip()
        return out if r.returncode == 0 and out and out != n else None

    with ThreadPoolExecutor(16) as ex:
        refs = list(ex.map(ref, names))
    ok = bad = only_ref = only_mine = 0
    for n, want in zip(names, refs):
        sym = demangle(n)
        got = full_signature(sym) if sym else None
        if want is None and got is None:
            continue
        if want == got:
            ok += 1
        elif want is None:
            only_mine += 1
            print(f"MINE ONLY  {n}\n    {got}")
        elif got is None:
            only_ref += 1
            print(f"DTK ONLY   {n}\n    {want}")
        else:
            bad += 1
            print(f"DIFFER     {n}\n    dtk:  {want}\n    mine: {got}")
    print(f"\nmatch {ok}, differ {bad}, dtk-only {only_ref}, mine-only {only_mine}")


if __name__ == "__main__":
    import sys
    if "--check" in sys.argv:
        _check()
    else:
        for arg in sys.argv[1:]:
            s = demangle(arg)
            print(full_signature(s) if s else arg)
