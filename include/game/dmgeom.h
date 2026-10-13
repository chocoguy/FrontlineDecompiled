// dmgeom: which variant of each switchable DM mesh node an object shows,
// and the resulting part list. Hand-maintained.
#ifndef _GAME_DMGEOM_H_
#define _GAME_DMGEOM_H_

#include "types.h"
#include "game_types.h"

class CDMesh;
#define DMGEOM_MAX_NODES 32

// Which variant each switchable node shows (0xFF: none). size 0x24
struct DMGeometryState_T {
    /* 0x0 */ u32 m_dirty;  // bit per node
    /* 0x4 */ u8 m_variant[DMGEOM_MAX_NODES];
};

// A part to draw, optionally with its own display list (the morph blend
// puts the blended list in the first part). size 0x8
struct DMGeomPartData_T {
    /* 0x0 */ u16 m_part;
    /* 0x2 */ s16 m_listSize;
    /* 0x4 */ u8* m_list;  // NULL: the part's own
};

// Free functions
void DMGeomSetNodeState(DMGeometryState_T*, long node, long variant);  // 0x800F6DB8
void DMGeomResetState(DMGeometryState_T*);  // 0x800F6DE4
int DMGeomBuildPartList(DMGeomPartData_T*, CDMesh*, DMGeometryState_T*);  // 0x800F6EDC

#endif
