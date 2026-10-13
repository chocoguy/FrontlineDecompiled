#!/usr/bin/env python3

###
# Generates build files for the project.
# This file also includes the project configuration,
# such as compiler flags and the object matching status.
#
# Usage:
#   python3 configure.py
#   ninja
#
# Append --help to see available options.
###

import argparse
import sys
from pathlib import Path
from typing import Any, Dict, List

from tools.project import (
    Object,
    ProgressCategory,
    ProjectConfig,
    calculate_progress,
    generate_build,
    is_windows,
)

# Game versions
DEFAULT_VERSION = 0
VERSIONS = [
    "GMFE69",  # 0
]

parser = argparse.ArgumentParser()
parser.add_argument(
    "mode",
    choices=["configure", "progress"],
    default="configure",
    help="script mode (default: configure)",
    nargs="?",
)
parser.add_argument(
    "-v",
    "--version",
    choices=VERSIONS,
    type=str.upper,
    default=VERSIONS[DEFAULT_VERSION],
    help="version to build",
)
parser.add_argument(
    "--build-dir",
    metavar="DIR",
    type=Path,
    default=Path("build"),
    help="base build directory (default: build)",
)
parser.add_argument(
    "--binutils",
    metavar="BINARY",
    type=Path,
    help="path to binutils (optional)",
)
parser.add_argument(
    "--compilers",
    metavar="DIR",
    type=Path,
    help="path to compilers (optional)",
)
parser.add_argument(
    "--map",
    action="store_true",
    help="generate map file(s)",
)
parser.add_argument(
    "--debug",
    action="store_true",
    help="build with debug info (non-matching)",
)
if not is_windows():
    parser.add_argument(
        "--wrapper",
        metavar="BINARY",
        type=Path,
        help="path to wibo or wine (optional)",
    )
parser.add_argument(
    "--dtk",
    metavar="BINARY | DIR",
    type=Path,
    help="path to decomp-toolkit binary or source (optional)",
)
parser.add_argument(
    "--objdiff",
    metavar="BINARY | DIR",
    type=Path,
    help="path to objdiff-cli binary or source (optional)",
)
parser.add_argument(
    "--sjiswrap",
    metavar="EXE",
    type=Path,
    help="path to sjiswrap.exe (optional)",
)
parser.add_argument(
    "--ninja",
    metavar="BINARY",
    type=Path,
    help="path to ninja binary (optional)",
)
parser.add_argument(
    "--verbose",
    action="store_true",
    help="print verbose output",
)
parser.add_argument(
    "--non-matching",
    dest="non_matching",
    action="store_true",
    help="builds equivalent (but non-matching) or modded objects",
)
parser.add_argument(
    "--shift-test",
    dest="shift_test",
    action="store_true",
    help="link src/shift_test.c first to move all code and data (implies --non-matching)",
)
parser.add_argument(
    "--warn",
    dest="warn",
    type=str,
    choices=["all", "off", "error"],
    help="how to handle warnings",
)
parser.add_argument(
    "--no-progress",
    dest="progress",
    action="store_false",
    help="disable progress calculation",
)
args = parser.parse_args()

config = ProjectConfig()
config.version = str(args.version)
version_num = VERSIONS.index(config.version)

# Apply arguments
config.build_dir = args.build_dir
config.dtk_path = args.dtk
config.objdiff_path = args.objdiff
config.binutils_path = args.binutils
config.compilers_path = args.compilers
config.generate_map = args.map
config.non_matching = args.non_matching or args.shift_test
config.sjiswrap_path = args.sjiswrap
config.ninja_path = args.ninja
config.progress = args.progress
if not is_windows():
    config.wrapper = args.wrapper
# Don't build asm unless we're --non-matching
if not config.non_matching:
    config.asm_dir = None

# Tool versions
config.binutils_tag = "2.42-2"
config.compilers_tag = "20251118"
config.dtk_tag = "v1.8.3"
config.objdiff_tag = "v3.6.1"
config.sjiswrap_tag = "v1.2.2"
config.wibo_tag = "1.0.3"

# Project
config.config_path = Path("config") / config.version / "config.yml"
config.check_sha_path = Path("config") / config.version / "build.sha1"
config.asflags = [
    "-mgekko",
    "--strip-local-absolute",
    "-I include",
    f"-I build/{config.version}/include",
    f"--defsym BUILD_VERSION={version_num}",
]
config.ldflags = [
    "-fp hardware",
    "-nodefaults",
]
if args.debug:
    config.ldflags.append("-g")  # Or -gdwarf-2 for Wii linkers
if args.map:
    config.ldflags.append("-mapunused")
    # config.ldflags.append("-listclosure") # For Wii linkers

# Use for any additional files that should cause a re-configure when modified
config.reconfig_deps = []

# Optional numeric ID for decomp.me preset
# Can be overridden in libraries or objects
config.scratch_preset_id = None

# Base flags, common to most GC/Wii games.
# Generally leave untouched, with overrides added below.
cflags_base = [
    "-nodefaults",
    "-proc gekko",
    "-align powerpc",
    "-enum int",
    "-fp hardware",
    "-Cpp_exceptions off",
    "-O4,p",
    "-inline auto",
    '-pragma "cats off"',
    '-pragma "warn_notinlined off"',
    "-maxerrors 1",
    "-nosyspath",
    "-RTTI off",
    "-fp_contract on",
    "-str reuse",
    "-multibyte",  # For Wii compilers, replace with `-enc SJIS`
    "-i include",
    f"-i build/{config.version}/include",
    f"-DBUILD_VERSION={version_num}",
    f"-DVERSION_{config.version}",
]

# Debug flags
if args.debug:
    # Or -sym dwarf-2 for Wii compilers
    cflags_base.extend(["-sym on", "-DDEBUG=1"])
else:
    cflags_base.append("-DNDEBUG=1")

# Warning flags
if args.warn == "all":
    cflags_base.append("-W all")
elif args.warn == "off":
    cflags_base.append("-W off")
elif args.warn == "error":
    cflags_base.append("-W error")

# Metrowerks library flags
cflags_runtime = [
    *cflags_base,
    "-use_lmw_stmw on",
    "-str reuse,pool,readonly",
    "-gccinc",
    "-common off",
    "-inline auto",
]

# REL flags
cflags_rel = [
    *cflags_base,
    "-sdata 0",
    "-sdata2 0",
]

# Game and EA library flags. The game ships C++ exception tables and RTTI, so
# both are enabled. String literals live in .rodata/.sdata2 in the retail
# binary, which needs readonly strings (verified on game/MallocInit.cpp).
# Registers are saved with stmw/lmw (verified on game/configfile.cpp).
# Inlining is deferred: functions can inline ones defined later in the file and
# are emitted in reverse order (verified on game/toilet.cpp).
# The compiler version is still a guess (GC/1.3.2 gave identical code there).
cflags_game = [
    *cflags_base,
    "-Cpp_exceptions on",
    "-RTTI on",
    "-str reuse,readonly",
    "-use_lmw_stmw on",
    "-inline auto,deferred",
]

# EA library flags (EA System, EA Sound): registers are saved through
# _savegpr/_restgpr, products are never fused into fmadds, and inlining is
# not deferred (functions come out in source order; verified on
# ea/sys/fontdraw.cpp).
cflags_ea = [
    *cflags_base,
    "-Cpp_exceptions on",
    "-RTTI on",
    "-str reuse,readonly",
    "-use_lmw_stmw off",
    "-fp_contract off",
]

config.linker_version = "GC/1.3.2"


# Helper function for Dolphin libraries
def DolphinLib(lib_name: str, objects: List[Object]) -> Dict[str, Any]:
    return {
        "lib": lib_name,
        "mw_version": "GC/1.2.5n",
        # The SDK was built without FP contraction (no fused multiply-adds;
        # verified on GXSetViewportJitter).
        "cflags": [*cflags_base, "-fp_contract off"],
        "progress_category": "sdk",
        "objects": objects,
    }


# Helper function for REL script objects
def Rel(lib_name: str, objects: List[Object]) -> Dict[str, Any]:
    return {
        "lib": lib_name,
        "mw_version": "GC/1.3.2",
        "cflags": cflags_rel,
        "progress_category": "game",
        "objects": objects,
    }


Matching = True                   # Object matches and should be linked
NonMatching = False               # Object does not match and should not be linked
Equivalent = config.non_matching  # Object should be linked when configured with --non-matching


# Object is only matching for specific versions
def MatchingFor(*versions):
    return config.version in versions


config.warn_missing_config = True
config.warn_missing_source = False
config.libs = [
    {
        "lib": "Game",
        "mw_version": "GC/1.3.2",
        "cflags": cflags_game,
        "progress_category": "game",
        "objects": [
            Object(Equivalent, "game/Moh2.cpp"),
            Object(Equivalent, "game/real_bridge.cpp"),
            Object(Equivalent, "game/Screen.cpp"),
            Object(Equivalent, "game/surfacetype.cpp"),
            Object(Equivalent, "game/volume_query_test.cpp"),
            Object(Equivalent, "game/configfile.cpp"),
            Object(Equivalent, "game/debug_picker.cpp"),
            Object(Equivalent, "game/filesys.cpp"),
            Object(Matching, "game/MallocInit.cpp"),
            Object(Equivalent, "game/memory.cpp"),
            Object(Equivalent, "game/system.cpp"),
            Object(Equivalent, "game/toilet.cpp"),
            Object(Equivalent, "game/bsbifunc.cpp"),
            Object(Equivalent, "game/bsevents.cpp"),
            Object(Equivalent, "game/bsfile.cpp"),
            Object(Equivalent, "game/bsmachin.cpp"),
            Object(Equivalent, "game/bsmessage.cpp"),
            Object(Equivalent, "game/bsobject.cpp"),
            Object(Equivalent, "game/bsschedule.cpp"),
            Object(Equivalent, "game/bstimer.cpp"),
            Object(Equivalent, "game/bsutil.cpp"),
            Object(Equivalent, "game/objcreate.cpp"),
            Object(Equivalent, "game/propdat.cpp"),
            Object(Equivalent, "game/proxtrig.cpp"),
            Object(Equivalent, "game/soundschedule.cpp"),
            Object(Equivalent, "game/AIObject.cpp"),
            Object(Equivalent, "game/AIPlayerObject.cpp"),
            Object(Equivalent, "game/AISoldierObject.cpp"),
            Object(Equivalent, "game/AISplinePath.cpp"),
            Object(Equivalent, "game/AIStatus.cpp"),
            Object(Equivalent, "game/AITankObject.cpp"),
            Object(Equivalent, "game/CoverPoint.cpp"),
            Object(Equivalent, "game/LinkedList.cpp"),
            Object(Equivalent, "game/ObstacleAvoidance.cpp"),
            Object(Equivalent, "game/PathNodeList.cpp"),
            Object(Equivalent, "game/SearchNode.cpp"),
            Object(Equivalent, "game/SearchNodeList.cpp"),
            Object(Equivalent, "game/buddy.cpp"),
            Object(Equivalent, "game/AIDoodad.cpp"),
            Object(Equivalent, "game/AIPlayerDoodad.cpp"),
            Object(Equivalent, "game/AISoldierDoodad.cpp"),
            Object(Equivalent, "game/AITankDoodad.cpp"),
            Object(Equivalent, "game/alarm.cpp"),
            Object(Equivalent, "game/AIFilter.cpp"),
            Object(Equivalent, "game/anim_callback.cpp"),
            Object(Equivalent, "game/animated.cpp"),
            Object(Equivalent, "game/animchan.cpp"),
            Object(Equivalent, "game/animdb.cpp"),
            Object(Equivalent, "game/animemot.cpp"),
            Object(Equivalent, "game/animintf.cpp"),
            Object(Equivalent, "game/animmorph.cpp"),
            Object(Equivalent, "game/animmot.cpp"),
            Object(Equivalent, "game/animseq.cpp"),
            Object(Equivalent, "game/animskinned.cpp"),
            Object(Equivalent, "game/animst.cpp"),
            Object(Equivalent, "game/animtran.cpp"),
            Object(Equivalent, "game/animwgt.cpp"),
            Object(Equivalent, "game/anim.cpp"),
            Object(Equivalent, "game/skeleton.cpp"),
            Object(Equivalent, "game/skelblend.cpp"),
            Object(Equivalent, "game/cdbgeom.cpp"),
            Object(Equivalent, "game/cdbobject.cpp"),
            Object(Equivalent, "game/clsn_table.cpp"),
            Object(Equivalent, "game/location_collision.cpp"),
            Object(Equivalent, "game/compartment.cpp"),
            Object(Equivalent, "game/draw_context.cpp"),
            Object(Equivalent, "game/light.cpp"),
            Object(Equivalent, "game/lightvolman.cpp"),
            Object(Equivalent, "game/viewport.cpp"),
            Object(Equivalent, "game/camera.cpp"),
            Object(Equivalent, "game/mainloop.cpp"),
            Object(Equivalent, "game/fader.cpp"),
            Object(Equivalent, "game/font.cpp"),
            Object(Equivalent, "game/motionblur.cpp"),
            Object(Equivalent, "game/particlesystem.cpp"),
            Object(Equivalent, "game/shadow.cpp"),
            Object(Equivalent, "game/sprite.cpp"),
            Object(Equivalent, "game/texpack.cpp"),
            Object(Equivalent, "game/texture.cpp"),
            Object(Equivalent, "game/decal.cpp"),
            Object(Equivalent, "game/devicemgr.cpp"),
            Object(Equivalent, "game/UISError.c"),
            Object(Equivalent, "game/keymap.cpp"),
            Object(Equivalent, "game/input.cpp"),
            Object(Equivalent, "game/UISActionProcess.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "game/UISEvent.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "game/UISStack.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "game/UIStudio.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "game/UISUtils.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "game/ispauselibrary.cpp"),
            Object(Equivalent, "game/ispausegroup.cpp"),
            Object(Equivalent, "game/isShellLibrary.cpp"),
            Object(Equivalent, "game/isShellGroup.cpp"),
            Object(Equivalent, "game/isexportdefs.cpp"),
            Object(Equivalent, "game/IStudio.cpp"),
            Object(Equivalent, "game/IStudioFuncs.cpp"),
            Object(Equivalent, "game/TibPolygon.cpp"),
            Object(Equivalent, "game/TibText.cpp"),
            Object(Equivalent, "game/GameMessage.cpp"),
            Object(Equivalent, "game/matrix.cpp"),
            Object(Equivalent, "game/matstack.cpp"),
            Object(Equivalent, "game/plane.cpp"),
            Object(Equivalent, "game/quaternion.cpp"),
            Object(Equivalent, "game/trig.cpp"),
            Object(Equivalent, "game/vector.cpp"),
            Object(Equivalent, "game/line.cpp"),
            Object(Equivalent, "game/MathFun.cpp"),
            Object(Equivalent, "game/anim_obj.cpp"),
            Object(Equivalent, "game/AnimObjFactory.cpp"),
            Object(Equivalent, "game/attachobject.cpp"),
            Object(Equivalent, "game/collisionvolume.cpp"),
            Object(Equivalent, "game/hearingvolume.cpp"),
            Object(Equivalent, "game/hierobject.cpp"),
            Object(Equivalent, "game/jaw_controller.cpp"),
            Object(Equivalent, "game/player.cpp"),
            Object(Equivalent, "game/player_weapon_object.cpp"),
            Object(Equivalent, "game/skybox.cpp"),
            Object(Equivalent, "game/Soldier_object.cpp"),
            Object(Equivalent, "game/static_obj.cpp"),
            Object(Equivalent, "game/staticobjfactory.cpp"),
            Object(Equivalent, "game/Tank_object.cpp"),
            Object(Equivalent, "game/thrown_obj.cpp"),
            Object(Equivalent, "game/world_object.cpp"),
            Object(Equivalent, "game/orient.cpp"),
            Object(Equivalent, "game/pumhandler.cpp"),
            Object(Equivalent, "game/UserInterface.cpp"),
            Object(Equivalent, "game/popup.cpp"),
            Object(Equivalent, "game/animated_volume.cpp"),
            Object(Equivalent, "game/box.cpp"),
            Object(Equivalent, "game/capsule.cpp"),
            Object(Equivalent, "game/collision.cpp"),
            Object(Equivalent, "game/csg_volume.cpp"),
            Object(Equivalent, "game/fiber.cpp"),
            Object(Equivalent, "game/sphere.cpp"),
            Object(Equivalent, "game/world_volume.cpp"),
            Object(Equivalent, "game/bullet.cpp"),
            Object(Equivalent, "game/bulletfactory.cpp"),
            Object(Equivalent, "game/explosion.cpp"),
            Object(Equivalent, "game/impact.cpp"),
            Object(Equivalent, "game/projectilebullet.cpp"),
            Object(Equivalent, "game/thrownbullet.cpp"),
            Object(Equivalent, "game/weapon.cpp"),
            Object(Equivalent, "game/weaponfactory.cpp"),
            Object(Equivalent, "game/publisher.cpp"),
            Object(Equivalent, "game/scene.cpp"),
            Object(Equivalent, "game/destructor_queue.cpp"),
            Object(Equivalent, "game/gcscratchpad.cpp"),
            Object(Equivalent, "game/Pause.cpp"),
            Object(Equivalent, "game/ShellMenu.cpp"),
            Object(Equivalent, "game/pathxSND.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "game/sound.cpp"),
            Object(Equivalent, "game/soundtable.cpp"),
            Object(Equivalent, "game/MemoryCardGC.cpp"),
            Object(Equivalent, "game/gcSystem.cpp"),
            Object(Equivalent, "game/renderlist.cpp"),
            Object(Equivalent, "game/staticmesh.cpp"),
            Object(Equivalent, "game/debug_prof.cpp"),
            Object(Equivalent, "game/dmrender.cpp"),
            Object(Equivalent, "game/dmcluster.cpp"),
            Object(Equivalent, "game/dmesh.cpp"),
            Object(Equivalent, "game/dmgeom.cpp"),
            Object(Equivalent, "game/dmmorph.cpp"),
            Object(Equivalent, "game/ConfigGC.cpp"),
            Object(Equivalent, "game/audioplayer.cpp"),
            Object(Equivalent, "game/loadelf.cpp"),
            Object(Equivalent, "game/dvdStatus_base.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "game/animplayer.cpp"),
            Object(Equivalent, "game/path.c", extra_cflags=["-lang=c++"]),
        ],
    },
    {
        "lib": "Movie",
        "mw_version": "GC/1.3.2",
        "cflags": cflags_game,
        "progress_category": "game",
        "objects": [
            Object(Equivalent, "movie/avplayer.cpp"),
            Object(Equivalent, "movie/bigswizzler.cpp"),
            Object(Equivalent, "movie/bigyuvswizzler.cpp"),
            Object(Equivalent, "movie/framework.cpp"),
            Object(Equivalent, "movie/gc_idct.asm"),
            Object(Equivalent, "movie/getbits.cpp"),
            Object(Equivalent, "movie/getblk.cpp"),
            Object(Equivalent, "movie/gethdr.cpp"),
            Object(Equivalent, "movie/getpic.cpp"),
            Object(Equivalent, "movie/getvlc.cpp"),
            Object(Equivalent, "movie/hwvideodisplaylist.cpp"),
            Object(Equivalent, "movie/idct.cpp"),
            Object(Equivalent, "movie/motion.cpp"),
            Object(Equivalent, "movie/rcmp_main.cpp"),
            Object(Equivalent, "movie/rcmp_mpc_codec.cpp"),
            Object(Equivalent, "movie/rcmp_mpc_codec_chunk_types.cpp"),
            Object(Equivalent, "movie/rcmp2real.cpp"),
            Object(Equivalent, "movie/rcmpbase.cpp"),
            Object(Equivalent, "movie/recon.cpp"),
            Object(Equivalent, "movie/swvideodisplaylist.cpp"),
        ],
    },
    {
        "lib": "EA System",
        "mw_version": "GC/1.3.2",
        "cflags": cflags_ea,
        "progress_category": "ea",
        "objects": [
            Object(Equivalent, "ea/sys/filesysopts.cpp"),
            Object(Equivalent, "ea/sys/filedev.cpp"),
            Object(Equivalent, "ea/sys/filesys.cpp"),
            Object(Equivalent, "ea/sys/syncfile.cpp"),
            Object(Equivalent, "ea/sys/locatbig.cpp"),
            Object(Equivalent, "ea/sys/hlafile.cpp"),
            Object(Equivalent, "ea/sys/stream.cpp"),
            Object(Equivalent, "ea/sys/hlsfile.cpp"),
            Object(Equivalent, "ea/sys/fontinit.cpp"),
            Object(Equivalent, "ea/sys/fontcreate.cpp"),
            Object(Equivalent, "ea/sys/fontdriver.cpp"),
            Object(Equivalent, "ea/sys/fontstdf.cpp"),
            Object(Equivalent, "ea/sys/fontchar.cpp"),
            Object(Equivalent, "ea/sys/fontdraw.cpp"),
            Object(Equivalent, "ea/sys/fontkern.cpp"),
            Object(Equivalent, "ea/sys/fontnull.cpp"),
            Object(Equivalent, "ea/sys/pad.cpp"),
            Object(Equivalent, "ea/sys/ipad.cpp"),
            Object(Equivalent, "ea/sys/memvars.cpp"),
            Object(Equivalent, "ea/sys/random.cpp"),
            Object(Equivalent, "ea/sys/memclass.cpp"),
            Object(Equivalent, "ea/sys/memunused.cpp"),
            Object(Equivalent, "ea/sys/memalloc.cpp"),
            Object(Equivalent, "ea/sys/mbdefault.cpp"),
            Object(Equivalent, "ea/sys/memfree.cpp"),
            Object(Equivalent, "ea/sys/resize.cpp"),
            Object(Equivalent, "ea/sys/meminit.cpp"),
            Object(Equivalent, "ea/sys/meminitadr.cpp"),
            Object(Equivalent, "ea/sys/memrestore.cpp"),
            Object(Equivalent, "ea/sys/memblock.cpp"),
            Object(Equivalent, "ea/sys/memlist.cpp"),
            Object(Equivalent, "ea/sys/memstd.cpp"),
            Object(Equivalent, "ea/sys/bmem.cpp"),
            Object(Equivalent, "ea/sys/aramalloc.cpp"),
            Object(Equivalent, "ea/sys/initosalloc.cpp"),
            Object(Equivalent, "ea/sys/memcopy.cpp"),
            Object(Equivalent, "ea/sys/memfill.cpp"),
            Object(Equivalent, "ea/sys/memmove.cpp"),
            Object(Equivalent, "ea/sys/memclear.cpp"),
            Object(Equivalent, "ea/sys/abortmsg.cpp"),
            Object(Equivalent, "ea/sys/debugger.cpp"),
            Object(Equivalent, "ea/sys/printn3.cpp"),
            Object(Equivalent, "ea/sys/printstr.cpp"),
            Object(Equivalent, "ea/sys/exit.cpp"),
            Object(Equivalent, "ea/sys/systemvars.cpp"),
            Object(Equivalent, "ea/sys/stricmp.cpp"),
            Object(Equivalent, "ea/sys/cpudetect.cpp"),
            Object(Equivalent, "ea/sys/inittmr.cpp"),
            Object(Equivalent, "ea/sys/initvblt.cpp"),
            Object(Equivalent, "ea/sys/timer.cpp"),
            Object(Equivalent, "ea/sys/timerthread.cpp"),
            Object(Equivalent, "ea/sys/threads.cpp"),
            Object(Equivalent, "ea/sys/mutex2.cpp"),
            Object(Equivalent, "ea/sys/systask.cpp"),
            Object(Equivalent, "ea/sys/signals.cpp"),
        ],
    },
    {
        "lib": "EA Sound",
        "mw_version": "GC/1.3.2",
        "cflags": cflags_ea,
        "progress_category": "ea",
        "objects": [
            Object(Equivalent, "ea/snd/sauthor.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/ssysinit.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/ssysserv.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/ssysreal.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/sserver.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/sclnt100.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/susercb.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/srender.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/saems.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/saemsbnk.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/saemsstr.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/saemsamb.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/saemsmbf.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/smemhigh.cpp"),
            Object(Equivalent, "ea/snd/smemman.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/slinklst.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/smath64.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/spoutlat.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/sballoc.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/sbadd.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/sbremove.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/sbvalid.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/sbhdrsze.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/sbhdrcpy.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/sbpatinf.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/sresopat.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/sbasync.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/sbplay.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/spktplay.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/spktctoh.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/sst.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/sdata.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/sstgetrp.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/sstovrhd.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/sststat.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/sstrstat.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/ssthold.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/sstqreqi.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/sstqmem.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/sstcrtap.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/sstvol.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/sstautov.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/sstgetpv.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/sst3dpos.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/sstpmult.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/sstrmdry.cpp"),
            Object(Equivalent, "ea/snd/sstfxlev.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/sstlowp.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/ssthighp.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/ssttmul.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/sstsetgl.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/smasterv.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/seffect.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/sautovol.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/svol.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/sgetpvol.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/sctrldry.cpp"),
            Object(Equivalent, "ea/snd/spitch.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/stimemul.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/sfxlevel.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/slowpass.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/shipass.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/sstop.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/sstopall.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/sover.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/stimerem.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/s3dlow.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/sctlfilt.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/splysdef.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/sattrdef.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/salloc.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/scalcfx.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/slib.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/srrange.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/sclcptch.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/spatkey.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/sgetdata.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/sgettag.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/stpparse.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/stagpat.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/smixvec.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/spat2hdr.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/satospkr.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/spantoaz.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/ssine.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/srandom.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/SNDI_sin.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/SNDI_cos.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/SNDI_root1x.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/SNDI_findprime.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/SNDI_mult16.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/sexithndl.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/slinkmix.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/smixer.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/smixfx.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/smixfram.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/smixptch.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/smixtmul.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/smixhip.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/smixlowp.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/sfilter.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/sinit16.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/sinitxa.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/sinitut.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/supmutf.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/supmutpf.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/supf.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/suplf.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/suppf.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/sx87d16.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/sdecmut.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/supxaf.cpp"),
            Object(Equivalent, "ea/snd/supxalf.cpp"),
            Object(Equivalent, "ea/snd/supxapf.cpp"),
            Object(Equivalent, "ea/snd/sfft24.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/sflpf.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/sfir.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/sfir8.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/sflpffir8.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/sfhpffir8.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/sfbpffir8.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/sfamplf.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/sfmixer.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/sfreson.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/sfsplit.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/sfsrc.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/sfecho.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/stretch.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/sfrsf.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/scrsfl.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/smixc.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/sfxrevc.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/sfxrev.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/coda.cpp"),
            Object(Equivalent, "ea/snd/eaxadecf.cpp"),
            Object(Equivalent, "ea/snd/snddrv.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/saramman.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/sdspmix.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/smemcpy.c", extra_cflags=["-lang=c++"]),
            Object(Equivalent, "ea/snd/sdfx.c", extra_cflags=["-lang=c++"]),
        ],
    },
    DolphinLib(
        "ai",
        [
            Object(Equivalent, "dolphin/ai/ai.c"),
        ],
    ),
    DolphinLib(
        "ar",
        [
            Object(Equivalent, "dolphin/ar/ar.c"),
            Object(Equivalent, "dolphin/ar/arq.c"),
        ],
    ),
    DolphinLib(
        "ax",
        [
            Object(Equivalent, "dolphin/ax/AX.c"),
            Object(Equivalent, "dolphin/ax/AXAlloc.c"),
            Object(Equivalent, "dolphin/ax/AXAux.c"),
            Object(Equivalent, "dolphin/ax/AXCL.c"),
            Object(Equivalent, "dolphin/ax/AXOut.c"),
            Object(Equivalent, "dolphin/ax/AXSPB.c"),
            Object(Equivalent, "dolphin/ax/AXVPB.c"),
            Object(Equivalent, "dolphin/ax/DSPCode.c"),
            Object(Equivalent, "dolphin/ax/AXProf.c"),
        ],
    ),
    DolphinLib(
        "base",
        [
            Object(Equivalent, "dolphin/base/PPCArch.c"),
        ],
    ),
    DolphinLib(
        "card",
        [
            Object(Equivalent, "dolphin/card/CARDBios.c"),
            Object(Equivalent, "dolphin/card/CARDUnlock.c"),
            Object(Equivalent, "dolphin/card/CARDRdwr.c"),
            Object(Equivalent, "dolphin/card/CARDBlock.c"),
            Object(Equivalent, "dolphin/card/CARDDir.c"),
            Object(Equivalent, "dolphin/card/CARDCheck.c"),
            Object(Equivalent, "dolphin/card/CARDMount.c"),
            Object(Equivalent, "dolphin/card/CARDFormat.c"),
            Object(Equivalent, "dolphin/card/CARDOpen.c"),
            Object(Equivalent, "dolphin/card/CARDCreate.c"),
            Object(Equivalent, "dolphin/card/CARDRead.c"),
            Object(Equivalent, "dolphin/card/CARDWrite.c"),
            Object(Equivalent, "dolphin/card/CARDDelete.c"),
            Object(Equivalent, "dolphin/card/CARDStat.c"),
            Object(Equivalent, "dolphin/card/CARDStatEx.c"),
            Object(Equivalent, "dolphin/card/CARDNet.c"),
        ],
    ),
    DolphinLib(
        "db",
        [
            Object(Equivalent, "dolphin/db/db.c"),
        ],
    ),
    DolphinLib(
        "dsp",
        [
            Object(Equivalent, "dolphin/dsp/dsp.c"),
            Object(Equivalent, "dolphin/dsp/dsp_debug.c"),
            Object(Equivalent, "dolphin/dsp/dsp_task.c"),
        ],
    ),
    DolphinLib(
        "dvd",
        [
            Object(Equivalent, "dolphin/dvd/dvdlow.c"),
            Object(Equivalent, "dolphin/dvd/dvdfs.c"),
            Object(Equivalent, "dolphin/dvd/dvd.c"),
            Object(Equivalent, "dolphin/dvd/dvdqueue.c"),
            Object(Equivalent, "dolphin/dvd/dvderror.c"),
            Object(Equivalent, "dolphin/dvd/fstload.c"),
        ],
    ),
    DolphinLib(
        "exi",
        [
            Object(Equivalent, "dolphin/exi/EXIBios.c"),
            Object(Equivalent, "dolphin/exi/EXIUart.c"),
        ],
    ),
    DolphinLib(
        "gx",
        [
            Object(Equivalent, "dolphin/gx/GXInit.c"),
            Object(Equivalent, "dolphin/gx/GXFifo.c"),
            Object(Equivalent, "dolphin/gx/GXAttr.c"),
            Object(Equivalent, "dolphin/gx/GXMisc.c"),
            Object(Equivalent, "dolphin/gx/GXGeometry.c"),
            Object(Equivalent, "dolphin/gx/GXFrameBuf.c"),
            Object(Equivalent, "dolphin/gx/GXLight.c"),
            Object(Equivalent, "dolphin/gx/GXTexture.c"),
            Object(Equivalent, "dolphin/gx/GXBump.c"),
            Object(Equivalent, "dolphin/gx/GXTev.c"),
            Object(Equivalent, "dolphin/gx/GXPixel.c"),
            Object(Equivalent, "dolphin/gx/GXStubs.c"),
            Object(Equivalent, "dolphin/gx/GXDisplayList.c"),
            Object(Equivalent, "dolphin/gx/GXTransform.c"),
            Object(Equivalent, "dolphin/gx/GXPerf.c"),
        ],
    ),
    DolphinLib(
        "mtx",
        [
            Object(Equivalent, "dolphin/mtx/mtx.c"),
            Object(Equivalent, "dolphin/mtx/mtxvec.c"),
            Object(Equivalent, "dolphin/mtx/mtx44.c"),
        ],
    ),
    DolphinLib(
        "os",
        [
            Object(Equivalent, "dolphin/os/OS.c"),
            Object(Equivalent, "dolphin/os/OSAlarm.c"),
            Object(Equivalent, "dolphin/os/OSAlloc.c"),
            Object(Equivalent, "dolphin/os/OSArena.c"),
            Object(Equivalent, "dolphin/os/OSAudioSystem.c"),
            Object(Equivalent, "dolphin/os/OSCache.c"),
            Object(Equivalent, "dolphin/os/OSContext.c"),
            Object(Equivalent, "dolphin/os/OSError.c"),
            Object(Equivalent, "dolphin/os/OSFont.c"),
            Object(Equivalent, "dolphin/os/OSInterrupt.c"),
            Object(Equivalent, "dolphin/os/OSLink.c"),
            Object(Equivalent, "dolphin/os/OSMessage.c"),
            Object(Equivalent, "dolphin/os/OSMemory.c"),
            Object(Equivalent, "dolphin/os/OSMutex.c"),
            Object(Equivalent, "dolphin/os/OSReboot.c"),
            Object(Equivalent, "dolphin/os/OSReset.c"),
            Object(Equivalent, "dolphin/os/OSResetSW.c"),
            Object(Equivalent, "dolphin/os/OSRtc.c"),
            Object(Equivalent, "dolphin/os/OSSync.c"),
            Object(Equivalent, "dolphin/os/OSThread.c"),
            Object(Equivalent, "dolphin/os/OSTime.c"),
            Object(Equivalent, "dolphin/os/__start.c"),
            Object(Equivalent, "dolphin/os/__ppc_eabi_init.cpp"),
        ],
    ),
    DolphinLib(
        "pad",
        [
            Object(Equivalent, "dolphin/pad/Padclamp.c"),
            Object(Equivalent, "dolphin/pad/Pad.c"),
        ],
    ),
    DolphinLib(
        "si",
        [
            Object(Equivalent, "dolphin/si/SIBios.c"),
            Object(Equivalent, "dolphin/si/SISamplingRate.c"),
        ],
    ),
    DolphinLib(
        "vi",
        [
            Object(Equivalent, "dolphin/vi/vi.c"),
        ],
    ),
    {
        "lib": "TRK_MINNOW_DOLPHIN",
        "mw_version": config.linker_version,
        # MetroTRK keeps all globals out of small data (lis/addi addressing,
        # 4-byte globals in .bss) and was built with deferred inlining
        # (functions emitted in reverse order; later helpers inlined).
        "cflags": [*cflags_runtime, "-sdata 0", "-sdata2 0", "-inline auto,deferred"],
        "progress_category": "sdk",
        "objects": [
            Object(Equivalent, "TRK_MINNOW_DOLPHIN/mainloop.c"),
            Object(Equivalent, "TRK_MINNOW_DOLPHIN/nubevent.c"),
            Object(Equivalent, "TRK_MINNOW_DOLPHIN/nubinit.c"),
            Object(Equivalent, "TRK_MINNOW_DOLPHIN/msg.c"),
            Object(Equivalent, "TRK_MINNOW_DOLPHIN/msgbuf.c"),
            Object(Equivalent, "TRK_MINNOW_DOLPHIN/serpoll.c"),
            Object(Equivalent, "TRK_MINNOW_DOLPHIN/usr_put.c"),
            Object(Equivalent, "TRK_MINNOW_DOLPHIN/dispatch.c"),
            Object(Equivalent, "TRK_MINNOW_DOLPHIN/msghndlr.c"),
            Object(Equivalent, "TRK_MINNOW_DOLPHIN/support.c"),
            Object(Equivalent, "TRK_MINNOW_DOLPHIN/mutex_TRK.c"),
            Object(Equivalent, "TRK_MINNOW_DOLPHIN/notify.c"),
            Object(Equivalent, "TRK_MINNOW_DOLPHIN/flush_cache.c"),
            Object(Equivalent, "TRK_MINNOW_DOLPHIN/mem_TRK.c"),
            Object(Equivalent, "TRK_MINNOW_DOLPHIN/targimpl.c"),
            Object(Equivalent, "TRK_MINNOW_DOLPHIN/targsupp.s"),
            Object(Equivalent, "TRK_MINNOW_DOLPHIN/__exception.s"),
            Object(Equivalent, "TRK_MINNOW_DOLPHIN/dolphin_trk.c"),
            Object(Equivalent, "TRK_MINNOW_DOLPHIN/mpc_7xx_603e.c"),
            Object(Equivalent, "TRK_MINNOW_DOLPHIN/main_TRK.c"),
            Object(Equivalent, "TRK_MINNOW_DOLPHIN/dolphin_trk_glue.c"),
            Object(Equivalent, "TRK_MINNOW_DOLPHIN/targcont.c"),
            Object(Equivalent, "TRK_MINNOW_DOLPHIN/target_options.c"),
            Object(Equivalent, "TRK_MINNOW_DOLPHIN/mslsupp.c"),
        ],
    },
    {
        "lib": "Runtime.PPCEABI.H",
        "mw_version": config.linker_version,
        "cflags": cflags_runtime,
        "progress_category": "sdk",
        "objects": [
            Object(Equivalent, "Runtime.PPCEABI.H/__va_arg.c"),
            Object(Equivalent, "Runtime.PPCEABI.H/global_destructor_chain.c"),
            Object(Equivalent, "Runtime.PPCEABI.H/CPlusLibPPC.cp"),
            Object(Equivalent, "Runtime.PPCEABI.H/NewMore.cp", extra_cflags=["-Cpp_exceptions on", "-RTTI on"]),
            Object(Equivalent, "Runtime.PPCEABI.H/NMWException.cp", extra_cflags=["-Cpp_exceptions on"]),
            Object(Equivalent, "Runtime.PPCEABI.H/ptmf.c"),
            Object(Equivalent, "Runtime.PPCEABI.H/runtime.c"),
            Object(Equivalent, "Runtime.PPCEABI.H/__init_cpp_exceptions.cpp"),
            Object(Equivalent, "Runtime.PPCEABI.H/Gecko_ExceptionPPC.cp", extra_cflags=["-Cpp_exceptions on", "-RTTI on"]),
            Object(Equivalent, "Runtime.PPCEABI.H/__mem.c"),
        ],
    },
    {
        "lib": "MSL_C.PPCEABI.bare.H",
        "mw_version": config.linker_version,
        # Deferred inlining: functions are emitted in reverse source order.
        "cflags": [*cflags_runtime, "-inline auto,deferred"],
        "progress_category": "sdk",
        "objects": [
            Object(Equivalent, "MSL_C.PPCEABI.bare.H/abort_exit.c"),
            Object(Equivalent, "MSL_C.PPCEABI.bare.H/alloc.c"),
            Object(Equivalent, "MSL_C.PPCEABI.bare.H/errno.c"),
            Object(Equivalent, "MSL_C.PPCEABI.bare.H/ansi_files.c"),
            Object(Equivalent, "MSL_C.PPCEABI.bare.H/ansi_fp.c"),
            Object(Equivalent, "MSL_C.PPCEABI.bare.H/bsearch.c"),
            Object(Equivalent, "MSL_C.PPCEABI.bare.H/buffer_io.c"),
            Object(Equivalent, "MSL_C.PPCEABI.bare.H/ctype.c"),
            Object(Equivalent, "MSL_C.PPCEABI.bare.H/direct_io.c"),
            Object(Equivalent, "MSL_C.PPCEABI.bare.H/file_io.c"),
            Object(Equivalent, "MSL_C.PPCEABI.bare.H/FILE_POS.C"),
            Object(Equivalent, "MSL_C.PPCEABI.bare.H/mbstring.c"),
            Object(Equivalent, "MSL_C.PPCEABI.bare.H/mem.c"),
            Object(Equivalent, "MSL_C.PPCEABI.bare.H/mem_funcs.c"),
            Object(Equivalent, "MSL_C.PPCEABI.bare.H/misc_io.c"),
            Object(Equivalent, "MSL_C.PPCEABI.bare.H/printf.c"),
            Object(Equivalent, "MSL_C.PPCEABI.bare.H/qsort.c"),
            Object(Equivalent, "MSL_C.PPCEABI.bare.H/rand.c"),
            Object(Equivalent, "MSL_C.PPCEABI.bare.H/float.c"),
            Object(Equivalent, "MSL_C.PPCEABI.bare.H/scanf.c"),
            Object(Equivalent, "MSL_C.PPCEABI.bare.H/signal.c"),
            Object(Equivalent, "MSL_C.PPCEABI.bare.H/string.c"),
            Object(Equivalent, "MSL_C.PPCEABI.bare.H/strtoul.c"),
            Object(Equivalent, "MSL_C.PPCEABI.bare.H/uart_console_io.c"),
            Object(Equivalent, "MSL_C.PPCEABI.bare.H/wchar_io.c"),
            Object(Equivalent, "MSL_C.PPCEABI.bare.H/e_acos.c"),
            Object(Equivalent, "MSL_C.PPCEABI.bare.H/e_asin.c"),
            Object(Equivalent, "MSL_C.PPCEABI.bare.H/e_atan2.c"),
            Object(Equivalent, "MSL_C.PPCEABI.bare.H/e_fmod.c"),
            Object(Equivalent, "MSL_C.PPCEABI.bare.H/e_pow.c"),
            Object(Equivalent, "MSL_C.PPCEABI.bare.H/e_rem_pio2.c"),
            Object(Equivalent, "MSL_C.PPCEABI.bare.H/k_cos.c"),
            Object(Equivalent, "MSL_C.PPCEABI.bare.H/k_rem_pio2.c"),
            Object(Equivalent, "MSL_C.PPCEABI.bare.H/k_sin.c"),
            Object(Equivalent, "MSL_C.PPCEABI.bare.H/k_tan.c"),
            Object(Equivalent, "MSL_C.PPCEABI.bare.H/s_atan.c"),
            Object(Equivalent, "MSL_C.PPCEABI.bare.H/s_copysign.c"),
            Object(Equivalent, "MSL_C.PPCEABI.bare.H/s_cos.c"),
            Object(Equivalent, "MSL_C.PPCEABI.bare.H/s_floor.c"),
            Object(Equivalent, "MSL_C.PPCEABI.bare.H/s_frexp.c"),
            Object(Equivalent, "MSL_C.PPCEABI.bare.H/s_ldexp.c"),
            Object(Equivalent, "MSL_C.PPCEABI.bare.H/s_modf.c"),
            Object(Equivalent, "MSL_C.PPCEABI.bare.H/s_sin.c"),
            Object(Equivalent, "MSL_C.PPCEABI.bare.H/s_tan.c"),
            Object(Equivalent, "MSL_C.PPCEABI.bare.H/w_acos.c"),
            Object(Equivalent, "MSL_C.PPCEABI.bare.H/w_asin.c"),
            Object(Equivalent, "MSL_C.PPCEABI.bare.H/w_atan2.c"),
            Object(Equivalent, "MSL_C.PPCEABI.bare.H/w_fmod.c"),
            Object(Equivalent, "MSL_C.PPCEABI.bare.H/w_pow.c"),
            Object(Equivalent, "MSL_C.PPCEABI.bare.H/math_ppc.c"),
        ],
    },
    {
        "lib": "amcstubs",
        "mw_version": "GC/1.2.5n",
        "cflags": cflags_base,
        "progress_category": "sdk",
        "objects": [
            Object(Equivalent, "amcstubs/AmcExi2Stubs.c"),
        ],
    },
    {
        "lib": "odenotstub",
        "mw_version": "GC/1.2.5n",
        "cflags": cflags_base,
        "progress_category": "sdk",
        "objects": [
            Object(Equivalent, "odenotstub/odenotstub.c"),
        ],
    },
    {
        "lib": "OdemuExi2",
        "mw_version": "GC/1.2.5n",
        # Deferred inlining: functions are emitted in reverse source order.
        "cflags": [*cflags_base, "-inline auto,deferred"],
        "progress_category": "sdk",
        "objects": [
            Object(Equivalent, "OdemuExi2/DebuggerDriver.c"),
        ],
    },
]

# Relocation safety check: pads every section so all original code and data
# move to new addresses. If the game still runs, non-matching code can be linked.
if args.shift_test:
    config.libs.append(
        {
            "lib": "ShiftTest",
            "mw_version": "GC/1.3.2",
            "cflags": cflags_base,
            "progress_category": "game",
            "objects": [
                Object(Equivalent, "shift_test.c"),
            ],
        }
    )


# Partially decompiled units (Object(..., partial=True)): src/<unit> holds the
# functions decompiled so far, and everything else still comes from dtk's split
# object, rewritten by tools/partial_obj.py so that references go to the
# source's definitions. The rest object is linked right after the unit.
partial_units = [
    obj.name
    for lib in config.libs
    for obj in lib["objects"]
    if obj.options.get("partial") and obj.completed
]
REST_SUFFIX = ".rest"
if config.non_matching and partial_units:
    out_dir = Path("build") / config.version
    config.custom_build_rules = [
        {
            "name": "partial_obj",
            "command": f"{sys.executable} tools/partial_obj.py $in -o $out",
            "description": "PARTIAL $out",
        }
    ]
    config.custom_build_steps = {"post-compile": []}
    config.link_order_objects = {}
    for unit in partial_units:
        stem = Path(unit).with_suffix("")
        rest = out_dir / "partial" / f"{stem}.o"
        config.custom_build_steps["post-compile"].append(
            {
                "outputs": rest,
                "rule": "partial_obj",
                "inputs": [out_dir / "obj" / f"{stem}.o", out_dir / "src" / f"{stem}.o"],
                "implicit": [Path("tools/partial_obj.py")],
            }
        )
        config.link_order_objects[unit + REST_SUFFIX] = str(rest)

# Game data listed under `extract:` in config.yml is written to
# build/<version>/include by the split step; make every compile wait for it.
config.custom_build_steps = config.custom_build_steps or {}
config.custom_build_steps.setdefault("pre-compile", []).append(
    {
        "outputs": "extract-assets",
        "rule": "phony",
        "inputs": [Path("build") / config.version / "config.json"],
    }
)

# Optional callback to adjust link order. This can be used to add, remove, or reorder objects.
# This is called once per module, with the module ID and the current link order.
def link_order_callback(module_id: int, objects: List[str]) -> List[str]:
    # Don't modify the link order for matching builds
    if not config.non_matching:
        return objects
    if module_id == 0:  # DOL
        if args.shift_test:
            # Link first so everything after it moves.
            objects = ["shift_test.c"] + [o for o in objects if o != "shift_test.c"]
        out = []
        for o in objects:
            out.append(o)
            if o in partial_units:
                out.append(o + REST_SUFFIX)
        return out
    return objects


config.link_order_callback = link_order_callback


# Optional extra categories for progress tracking
# Adjust as desired for your project
config.progress_categories = [
    ProgressCategory("game", "Game Code"),
    ProgressCategory("sdk", "SDK Code"),
    ProgressCategory("ea", "EA Libraries"),
]
config.progress_each_module = args.verbose
# Optional extra arguments to `objdiff-cli report generate`
config.progress_report_args = [
    # Marks relocations as mismatching if the target value is different
    # Default is "functionRelocDiffs=none", which is most lenient
    # "--config functionRelocDiffs=data_value",
]

if args.mode == "configure":
    # Write build.ninja and objdiff.json
    generate_build(config)
elif args.mode == "progress":
    # Print progress information
    calculate_progress(config)
else:
    sys.exit("Unknown mode: " + args.mode)
