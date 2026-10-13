# Frontline Decompiled

Medal of Honor: Frontline for the Gamecube decompiled!

GMFE69

KEEP IN MIND: This code is **functionally equivalent**, not byte-matching

This repository contains **no game data**. You need your own legally obtained
copy of the game to build and run it. The few binary pieces the code needs
(the disc-error font, the memory card icon, and the DSP microcode) are pulled
from your copy at build time.



## Requirements

- Python 3.8 or later
- [ninja](https://ninja-build.org/)
  - macOS: `brew install ninja`
  - Linux: your package manager, or `pip install ninja`
  - Windows: `pip install ninja`, or download it and add it to `PATH`
- [Dolphin](https://dolphin-emu.org/) to extract the disc and play the result
- A disc image of *Medal of Honor: Frontline* (USA, game ID `GMFE69`)

The compiler, [decomp-toolkit](https://github.com/encounter/decomp-toolkit), and
[wibo](https://github.com/decompals/wibo) (to run the Windows compiler on macOS
and Linux) are downloaded automatically by the build.

## Building

### 1. Extract the game

In Dolphin, right-click the game in your game list → **Properties** →
**Filesystem** tab → right-click the disc at the top → **Extract Entire Disc...**,
and pick `orig/GMFE69/disc` inside this repository. When it's done you should have:

```
orig/GMFE69/disc/sys/main.dol
orig/GMFE69/disc/files/Moh2RelGC.elf
...
```

On this disc, `sys/main.dol` is only a small loader. The real game is
`files/Moh2RelGC.elf`, and that's the file this project rebuilds.

### 2. Prepare the original executable

The project works from a DOL converted from the game's ELF. Download
decomp-toolkit (the version `configure.py` uses), convert the ELF, and keep a
copy of the ELF next to it:

```sh
python3 tools/download_tool.py dtk build/tools/dtk --tag v1.8.3
build/tools/dtk elf2dol orig/GMFE69/disc/files/Moh2RelGC.elf orig/GMFE69/Moh2RelGC.dol
cp orig/GMFE69/disc/files/Moh2RelGC.elf orig/GMFE69/Moh2RelGC.elf
```

(On Windows use `python`, `build\tools\dtk.exe`, and `copy` instead.)

The build checks the DOL's SHA-1 (`0e96c0bea0d7717297f376b7b11229ca99b4b9c6`)
and stops if your copy is a different version of the game.

### 3. Configure and build

```sh
python3 configure.py --non-matching --map
ninja
```

The first build downloads the compiler and other tools, then extracts the game
data the code needs from your DOL into `build/GMFE69/include/`.

The output is `build/GMFE69/Moh2RelGC.elf` (plus a `.dol` and a `.MAP`). You
can ignore the compiler warnings; the build fails if a unit doesn't compile.

Playing the build
-----------------

The game runs from a disc folder with your built ELF in place of the original.
The included script sets this up without touching your extracted files:

```sh
python3 tools/run_dolphin.py --stage-only
```

This creates `build/GMFE69/disc/` (with `sys/` and `files/`). In Dolphin, choose
**File → Open...** and open `build/GMFE69/disc/sys/main.dol`.

Run it without `--stage-only` to start Dolphin directly; pass `--dolphin <path>`
if Dolphin isn't at `/Applications/Dolphin.app` (the macOS default).

To do it by hand: copy the extracted disc folder, replace
`files/Moh2RelGC.elf` with `build/GMFE69/Moh2RelGC.elf`, and boot that copy's
`sys/main.dol`.

## Project Layout

| Path | Contents |
|---|---|
| `src/game/` | The game itself (AI, player, weapons, UI, rendering, scripting) |
| `src/ea/` | EA libraries: `sys` (file system, memory, fonts) and `snd` (sound and music) |
| `src/movie/` | Movie player and MPEG decoder |
| `src/dolphin/` | Nintendo Dolphin SDK |
| `src/MSL_C.PPCEABI.bare.H/`, `src/Runtime.PPCEABI.H/`, `src/TRK_MINNOW_DOLPHIN/` | Metrowerks C library, runtime, and debugger kernel |
| `include/` | Headers |
| `config/GMFE69/` | decomp-toolkit config: `symbols.txt`, `splits.txt`, `config.yml` |
| `tools/` | Build scripts and comparison/checking tools |
| `docs/` | Progress notes and lessons learned during the decompilation |

Things to know when changing code
---------------------------------

- **Rebuild after editing** with `ninja`; it only recompiles what changed.
- **Static data that's DMA'd** (audio, ARAM, GX, DSP buffers) must stay 32-byte
  aligned: declare it with `__attribute__((aligned(32)))`.
- **All sections are 32-byte aligned** in `config/GMFE69/splits.txt`. The retail
  loader reads the ELF's segments straight from disc, and a misaligned `.ctors`
  silently skips static constructors. Keep that alignment if you add units.
- **Shared struct layouts** are relied on everywhere. If you change a header,
  `python3 tools/regress.py` diffs every function's match against
  `docs/match_baseline.json` and catches accidental layout shifts.
- `tools/fncheck.py`, `callseq.py`, `abicheck.py`, `cmpcheck.py`,
  `constcheck.py`, and `offcheck.py` compare one unit against the original
  (they read `orig/GMFE69/Moh2RelGC.elf`). See `docs/progress_batch*.md` for
  how they were used.
- [objdiff](https://github.com/encounter/objdiff) works with this project for
  function-by-function diffs (`objdiff.json` is generated by `configure.py`).

## Credits

Project assisted by Claude
Built on the [decomp-toolkit project template](https://github.com/encounter/dtk-template).
Medal of Honor: Frontline is © Electronic Arts. This project is not affiliated
with or endorsed by Electronic Arts or Nintendo.
