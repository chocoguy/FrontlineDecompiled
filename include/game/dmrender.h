// dmrender: the per-object render state of deformable (DM) meshes:
// model binding, matrices, scale, lights, the cluster (skinning) and morph
// objects. Hand-maintained.
#ifndef _GAME_DMRENDER_H_
#define _GAME_DMRENDER_H_

#include "types.h"
#include "game_types.h"
#include "game/matrix.h"
#include "game/dmgeom.h"

class CDMesh;
class CMatrix;
struct DMClusterObject_T;
// Lights for a DM render object (size 0x64): up to three directional
// lights plus ambient (color[3]); colors are 0..255.
struct DMLighting_T {
    /* 0x00 */ f32 color[4][4];  // r, g, b, a
    /* 0x40 */ f32 dir[3][3];
};
struct DMMorphObject_T;
struct TUInt128;

enum {
    DMRENDER_DIRTY_MODEL = 0x1,
    DMRENDER_DIRTY_INIT = 0x410,
};

// size 0x260
struct DMRenderObject_T {
    /* 0x000 */ int m_valid;
    /* 0x004 */ u32 m_dirty;  // DMRENDER_DIRTY_*
    /* 0x008 */ u32 m_dirtyAll;  // accumulated
    /* 0x00C */ CDMesh* m_model;
    /* 0x010 */ int unk_0x10;
    /* 0x014 */ DMGeometryState_T m_geom;
    /* 0x038 */ u8 unk_0x38[0x8];
    /* 0x040 */ CMatrix m_viewProj;
    /* 0x080 */ CMatrix m_localToCamera;
    /* 0x0C0 */ float (*m_clusterMatrices)[3][4];
    /* 0x0C4 */ u32 unk_0xC4;
    /* 0x0C8 */ f32 m_scale[3];
    /* 0x0D4 */ u32 m_2dReject;
    /* 0x0D8 */ f32 m_lightDir[3][4];  // camera space, w = 1
    /* 0x108 */ f32 m_lightColor[3][4];
    /* 0x138 */ f32 m_ambient[4];  // rgb doubled
    /* 0x148 */ u8 unk_0x148[0x8];
    /* 0x150 */ int m_numParts;
    /* 0x154 */ DMGeomPartData_T m_parts[DMGEOM_MAX_NODES];
    /* 0x254 */ DMClusterObject_T* m_cluster;
    /* 0x258 */ DMMorphObject_T* m_morph;
    /* 0x25C */ u32 unk_0x25C;
};
SIZE_ASSERT(DMRenderObject_T, 0x260);

// Free functions
void DMLightingScaleAlpha(DMRenderObject_T*, float);  // 0x800F4A98
void DMLightingUpdate(DMRenderObject_T*, DMLighting_T*);  // 0x800F4AA8
void DMRenderSetClusterMatrices(DMRenderObject_T*, float(*)[3][4]);  // 0x800F4C74
void DMRenderBindMorphObj(DMRenderObject_T*, DMMorphObject_T*);  // 0x800F4C7C
void DMRenderBindClusterObj(DMRenderObject_T*, DMClusterObject_T*);  // 0x800F4C84
void DMRenderUpdateParts(DMRenderObject_T*);  // 0x800F4C8C
void DMRenderSetScale(DMRenderObject_T*, float, float, float);  // 0x800F4CE4
void DMRenderSetLocalToCamera(DMRenderObject_T*, CMatrix*, CMatrix*);  // 0x800F4CF4
void DMRenderSetViewProj(DMRenderObject_T*, CMatrix*);  // 0x800F4D18
void DMRenderSet2dReject(DMRenderObject_T*, unsigned long);  // 0x800F4D3C
void DMRenderResetState(DMRenderObject_T*);  // 0x800F4D50
void DMRenderBindModel(DMRenderObject_T*, CDMesh*);  // 0x800F4D98
void DMRenderInitObject(DMRenderObject_T*, TUInt128*, TUInt128*, unsigned long);  // 0x800F4DCC
void DMRenderSetFlags(DMRenderObject_T*, unsigned long);  // 0x800F4E3C
void DMRenderShutdown();  // 0x800F4E58
void DMRenderInit();  // 0x800F4E78

#endif
