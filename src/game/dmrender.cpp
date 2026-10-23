// dmrender: the per-object render state of deformable (DM) meshes.
#include "game/dmrender.h"
#include "game/dmcluster.h"
#include "game/dmgeom.h"
#include "game/matstack.h"
#include "game/matrix.h"
#include "string.h"

// Multiplies the ambient alpha (fading the object out).
void DMLightingScaleAlpha(DMRenderObject_T* obj, float scale) {
    obj->m_ambient[3] *= scale;
}

// Takes the lights (directions into the current matrix space).
void DMLightingUpdate(DMRenderObject_T* obj, DMLighting_T* lighting) {
    CVector3 dir[3];
    CVector3 in[3];
    for (int i = 0; i < 3; i++) {
        in[i].x = lighting->dir[i][0];
        in[i].y = lighting->dir[i][1];
        in[i].z = lighting->dir[i][2];
    }
    const CMatrix& m = g_matStack->m_stack[g_matStack->m_top];
    m.TransformVector(dir[0], in[0]);
    m.TransformVector(dir[1], in[1]);
    m.TransformVector(dir[2], in[2]);
    for (int i = 0; i < 3; i++) {
        obj->m_lightDir[i][0] = dir[i].x;
        obj->m_lightDir[i][1] = dir[i].y;
        obj->m_lightDir[i][2] = dir[i].z;
        obj->m_lightDir[i][3] = 1.0f;
        memcpy(obj->m_lightColor[i], lighting->color[i], sizeof(obj->m_lightColor[i]));
    }
    memcpy(obj->m_ambient, lighting->color[3], sizeof(obj->m_ambient));
    obj->m_ambient[0] *= 2.0f;
    obj->m_ambient[1] *= 2.0f;
    obj->m_ambient[2] *= 2.0f;
}

void DMRenderSetClusterMatrices(DMRenderObject_T* obj, float (*matrices)[3][4]) {
    obj->m_clusterMatrices = matrices;
}

void DMRenderBindMorphObj(DMRenderObject_T* obj, DMMorphObject_T* morph) {
    obj->m_morph = morph;
}

void DMRenderBindClusterObj(DMRenderObject_T* obj, DMClusterObject_T* cluster) {
    obj->m_cluster = cluster;
}

// Rebuilds the part list after the model or node states changed.
void DMRenderUpdateParts(DMRenderObject_T* obj) {
    if (obj->m_model != NULL && obj->m_geom.m_dirty != 0) {
        obj->m_numParts = DMGeomBuildPartList(obj->m_parts, obj->m_model, &obj->m_geom);
        obj->m_geom.m_dirty = 0;
    }
}

void DMRenderSetScale(DMRenderObject_T* obj, float x, float y, float z) {
    obj->m_scale[0] = x;
    obj->m_scale[1] = y;
    obj->m_scale[2] = z;
}

void DMRenderSetLocalToCamera(DMRenderObject_T* obj, CMatrix* localToCamera, CMatrix*) {
    obj->m_localToCamera = *localToCamera;
}

void DMRenderSetViewProj(DMRenderObject_T* obj, CMatrix* viewProj) {
    obj->m_viewProj = *viewProj;
}

void DMRenderSet2dReject(DMRenderObject_T* obj, unsigned long reject) {
    obj->m_2dReject = reject != 0;
}

void DMRenderResetState(DMRenderObject_T* obj) {
    memset(&obj->m_viewProj, 0,
           GC_SIZE(0x110, offsetof(DMRenderObject_T, m_numParts) - offsetof(DMRenderObject_T, m_viewProj)));
    obj->m_scale[0] = 1.0f;
    obj->m_scale[1] = 1.0f;
    obj->m_scale[2] = 1.0f;
}

void DMRenderBindModel(DMRenderObject_T* obj, CDMesh* model) {
    if (obj->m_model != model) {
        obj->m_model = model;
        obj->m_geom.m_dirty = 0xFFFFFFFF;
        obj->m_dirty |= DMRENDER_DIRTY_INIT | DMRENDER_DIRTY_MODEL;
        obj->m_dirtyAll |= DMRENDER_DIRTY_INIT | DMRENDER_DIRTY_MODEL;
    }
}

void DMRenderInitObject(DMRenderObject_T* obj, TUInt128*, TUInt128*, unsigned long) {
    memset(obj, 0, sizeof(DMRenderObject_T));
    obj->m_valid = 1;
    obj->m_model = NULL;
    obj->unk_0x10 = 0;
    obj->m_cluster = NULL;
    obj->m_morph = NULL;
    DMGeomResetState(&obj->m_geom);
    obj->m_dirty |= DMRENDER_DIRTY_INIT;
    obj->m_dirtyAll |= DMRENDER_DIRTY_INIT;
}

void DMRenderSetFlags(DMRenderObject_T* obj, unsigned long flags) {
    obj->m_dirty |= flags;
    obj->m_dirtyAll |= flags;
}

void DMRenderShutdown() {
    DMClusterShutdown();
}

void DMRenderInit() {
    DMClusterInit();
}
