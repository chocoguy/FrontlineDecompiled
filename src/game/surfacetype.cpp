#include "game/surfacetype.h"

#include "game/cdb.h"

static CDB* g_pCDB;

bool CSurfaceType::IsShootThru(int type) {
    return g_pCDB->surfaceTypes[type].flags & 1;
}

s16 CSurfaceType::GetSoundIndex(int type) {
    return g_pCDB->surfaceTypes[type].soundIndex;
}

u32 CSurfaceType::GetParticleCRC(int type) {
    return g_pCDB->surfaceTypes[type].particleCRC;
}

s16 CSurfaceType::GetDecalIndex(int type) {
    return g_pCDB->surfaceTypes[type].decalIndex;
}

void CSurfaceType::InitClass(CDB* cdb) {
    g_pCDB = cdb;
}
