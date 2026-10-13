// Skeleton for game/surfacetype (from tools/gen_headers.py, now hand-maintained).
#ifndef _GAME_SURFACETYPE_H_
#define _GAME_SURFACETYPE_H_

#include "types.h"
#include "game_types.h"

struct CDB;

// Per-surface properties from the level's collision database.
class CSurfaceType {
public:
    static bool IsShootThru(int type);      // 0x8001BFBC
    static s16 GetSoundIndex(int type);     // 0x8001BFE0
    static u32 GetParticleCRC(int type);    // 0x8001BFF8
    static s16 GetDecalIndex(int type);     // 0x8001C010
    static void InitClass(CDB* cdb);        // 0x8001C028
};

#endif
