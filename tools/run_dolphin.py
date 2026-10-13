#!/usr/bin/env python3

###
# Stage an extracted-disc folder containing the freshly built Moh2RelGC.elf
# and boot it in Dolphin.
#
# The retail main.dol is only a loader that reads Moh2RelGC.elf from the disc,
# so the staged folder is the original sys/ + files/ with that one file
# swapped. Dolphin boots a folder laid out as <root>/sys/main.dol +
# <root>/files/ as a virtual disc, so the real loader path is exercised.
#
# Original files are hard-linked into the staging folder (no extra disk use).
#
# Usage examples:
#   python tools/run_dolphin.py                 # stage + boot in Dolphin
#   python tools/run_dolphin.py --stage-only    # just refresh the staging folder
#   python tools/run_dolphin.py --log           # also open Dolphin's log window
###

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

DTK_ROOT = Path(__file__).resolve().parent.parent
REPO_ROOT = DTK_ROOT.parent
GAME_ELF = "Moh2RelGC.elf"
DEFAULT_DOLPHIN = Path("/Applications/Dolphin.app/Contents/MacOS/Dolphin")


def link_or_copy(src: Path, dst: Path) -> None:
    try:
        os.link(src, dst)
    except OSError:
        shutil.copy2(src, dst)


def mirror_tree(src_root: Path, dst_root: Path, skip: Path) -> None:
    """Hard-link every file under src_root into dst_root, except `skip`."""
    for src_dir, _dirs, files in os.walk(src_root):
        rel = Path(src_dir).relative_to(src_root)
        (dst_root / rel).mkdir(parents=True, exist_ok=True)
        for name in files:
            if name == ".DS_Store":
                continue
            src = Path(src_dir) / name
            if src == skip:
                continue
            dst = dst_root / rel / name
            if not dst.exists():
                link_or_copy(src, dst)


def stage(version: str, disc_src: Path, elf: Path) -> Path:
    if not elf.is_file():
        sys.exit(f"Built ELF not found: {elf} (run `ninja` first)")
    for required in ("sys/main.dol", "sys/boot.bin", "sys/bi2.bin",
                     "sys/apploader.img", f"files/{GAME_ELF}"):
        if not (disc_src / required).is_file():
            sys.exit(f"Missing {disc_src / required}; extract the disc there first")

    stage_root = DTK_ROOT / "build" / version / "disc"
    mirror_tree(disc_src / "sys", stage_root / "sys", skip=Path())
    mirror_tree(disc_src / "files", stage_root / "files",
                skip=disc_src / "files" / GAME_ELF)

    # Always replace the game ELF with a real copy of the build output, so a
    # later rebuild can never write through a hard link into the original.
    staged_elf = stage_root / "files" / GAME_ELF
    if staged_elf.exists() or staged_elf.is_symlink():
        staged_elf.unlink()
    shutil.copy2(elf, staged_elf)
    return stage_root


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("-v", "--version", default="GMFE69")
    parser.add_argument("--disc", type=Path,
                        help="folder containing the extracted sys/ and files/ "
                             "(default: orig/<version>/disc, else the folder above this project)")
    parser.add_argument("--elf", type=Path,
                        help="ELF to boot (default: build/<version>/Moh2RelGC.elf)")
    parser.add_argument("--dolphin", type=Path, default=DEFAULT_DOLPHIN)
    parser.add_argument("--user", type=Path,
                        help="separate Dolphin user folder (default: your normal one)")
    parser.add_argument("--log", action="store_true", help="open the log window")
    parser.add_argument("--debugger", action="store_true", help="show the debugger")
    parser.add_argument("--stage-only", action="store_true")
    parser.add_argument("dolphin_args", nargs="*",
                        help="extra arguments passed to Dolphin after `--`")
    args = parser.parse_args()

    disc = args.disc
    if disc is None:
        disc = DTK_ROOT / "orig" / args.version / "disc"
        if not disc.is_dir():
            disc = REPO_ROOT
    elf = args.elf or DTK_ROOT / "build" / args.version / GAME_ELF
    stage_root = stage(args.version, disc.resolve(), elf.resolve())
    main_dol = stage_root / "sys" / "main.dol"
    print(f"Staged {elf} -> {stage_root / 'files' / GAME_ELF}")
    if args.stage_only:
        return

    cmd = [str(args.dolphin), "-e", str(main_dol)]
    if args.user:
        cmd += ["-u", str(args.user.resolve())]
    if args.log:
        cmd.append("-l")
    if args.debugger:
        cmd.append("-d")
    cmd += args.dolphin_args
    print(" ".join(cmd))
    subprocess.run(cmd)


if __name__ == "__main__":
    main()
