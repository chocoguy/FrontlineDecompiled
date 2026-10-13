// CAnimated: the animation state of a character model (channels, state
// machine, weighted key frames) and drawing it with its lights.
#include "game/animated.h"
#include "game/Moh2.h"
#include "game/anim.h"
#include "game/anim_obj.h"
#include "game/animchan.h"
#include "game/animdb.h"
#include "game/animmot.h"
#include "game/animskinned.h"
#include "game/animst.h"
#include "game/bpdlight.h"
#include "game/dmcluster.h"
#include "game/dmesh.h"
#include "game/dmgeom.h"
#include "game/dmrender.h"
#include "game/draw_context.h"
#include "game/light.h"
#include "game/matrix.h"
#include "game/matstack.h"
#include "game/memory.h"
#include "math.h"
#include "string.h"

void CAnimated::InitClass() {
    CAnimSkinned::InitClass();
}

void CAnimated::Init(CAnimObject* owner, unsigned short) {
    m_pMesh = NULL;
    field_0xB90 = 0;
    AnimChanInitChannels(&m_hdr, m_channels);
    AnimStInitStructure((AnimStInfo_t*)m_stateInfo);
    m_pOwner = owner;
    m_hdr.field_0x8 = 0;
    field_0xA3C = 0x80;
    m_animFlags = 0;
    field_0xCC1 = false;
}

// Whether the database has a callback for `state` for this object type.
bool CAnimated::ValidState(unsigned short state) {
    if (g_AnimDB.GetStateCallbackID(state, m_hdr.objectType) == -1) {
        DebugMsg("WARNING: No callback fn found for lip synch 0x%X (Search in anim_ids.h for %d to "
                 "find missing anim\n)",
                 state, state);
        return false;
    }
    return true;
}

// Starts `state`. Returns 0xFFFF0000 if refused, 0xF0F0F0F0 with `next` set
// when another state must play first, else AnimStStartState's result.
int CAnimated::SetAnimationState(unsigned short state, float weight, unsigned short& next,
                                 long userValue, void* arg) {
    if (field_0xCC0 && !m_pOwner->field_0x24B0 &&
        g_AnimDB.GetStateCallbackID(state, m_hdr.objectType) == 3) {
        return 0xFFFF0000;
    }
    int pre = AnimStTransPreCheck((AnimStInfo_t*)m_stateInfo, &m_hdr, state);
    if (pre >= 0) {
        next = pre;
        return 0xF0F0F0F0;
    }
    field_0xCC1 = false;
    int result = AnimStStartState((AnimStInfo_t*)m_stateInfo, &m_hdr, m_channels, state, this,
                                  (int)arg, weight);
    *(long*)AnimStGetStateUserMemory((AnimStInfo_t*)m_stateInfo, state) = userValue;
    return result;
}

// Tells the owner a state finished (bit 15 of `state`: interrupted).
int CAnimated::FinishAnimationState(unsigned short state) {
    long userValue = *(long*)AnimStGetStateUserMemory((AnimStInfo_t*)m_stateInfo, state);
    if (userValue != -1) {
        m_pOwner->FinishAnimationState(userValue, (state >> 15) & 1);
    }
    return 0;
}

void CAnimated::UpdateAnimation(float dt) {
    AnimStProcess((AnimStInfo_t*)m_stateInfo, &m_hdr, m_channels, this, dt);
    AnimChanProcess(&m_hdr, m_channels, this, dt);
    m_pFrame = AnimMotCreateWeightList(&m_hdr, m_channels, (AnimStInfo_t*)m_stateInfo);
}

static inline void GetLightDirection(CLight* light, const CVector3& objPos, CVector3& dir) {
    light->GetPosition(dir);
    dir.x = dir.x - objPos.x;
    dir.y = dir.y - objPos.y;
    dir.z = dir.z - objPos.z;
}

void CAnimated::Draw(CDrawContext& ctx) {
    if (m_pMesh == NULL) {
        return;
    }
    DMRenderObject_T* render = (DMRenderObject_T*)m_render;
    DMRenderUpdateParts(render);

    CMatrix localToCamera;
    localToCamera.Multiply(*ctx.m_pWorld, *ctx.m_pView);
    g_matStack->Push();
    g_matStack->Load(&localToCamera);
    DMClusterSynthesizeMatrices(0, (DMClusterObject_T*)&m_numClusterMatrices, m_pMesh,
                                (CharOrient3_t*)m_pBaseFrame->jointData);
    g_matStack->Pop();

    DMRenderResetState(render);
    CMatrix viewProj;
    viewProj = *ctx.m_pProj;
    DMRenderSetViewProj(render, &viewProj);
    DMRenderSet2dReject(render, ctx.m_2dReject);
    DMRenderSetLocalToCamera(render, &localToCamera, NULL);
    DMRenderSetScale(render, ctx.m_scale[0], ctx.m_scale[1], ctx.m_scale[2]);
    DMRenderSetClusterMatrices(render, (float(*)[3][4])m_clusterMatrices);

    g_matStack->Push();
    g_matStack->Load(ctx.m_pView);
    BPDLightVolume* volume = m_pOwner->GetLightVolume();
    if (volume == NULL) {
        volume = CLight::GetDefaultLightVolume();
    }
    ctx.m_pLightVolume = volume;

    // directional lights and ambient from the light volume
    DMLighting_T lighting;
    BPDLight* light = volume->lights;
    memset(&lighting, 0, sizeof(lighting));
    int numDir = 0;
    lighting.color[0][3] = 255.0f;
    lighting.color[1][3] = 255.0f;
    lighting.color[2][3] = 255.0f;
    lighting.color[3][3] = 255.0f;
    for (int i = 0; i < volume->numLights; i++, light++) {
        float intensity = light->intensity;
        if (light->type == BPD_LIGHT_DIRECTIONAL) {
            lighting.dir[numDir][0] = light->dir[0];
            lighting.dir[numDir][1] = light->dir[1];
            lighting.dir[numDir][2] = light->dir[2];
            lighting.color[numDir][0] = 255.0f * light->color[0] * intensity;
            lighting.color[numDir][1] = 255.0f * light->color[1] * intensity;
            lighting.color[numDir][2] = 255.0f * light->color[2] * intensity;
            numDir++;
        } else {
            lighting.color[3][0] = 255.0f * light->color[0] * intensity;
            lighting.color[3][1] = 255.0f * light->color[1] * intensity;
            lighting.color[3][2] = 255.0f * light->color[2] * intensity;
        }
    }

    // the strongest nearby dynamic light takes a directional slot
    CLightDrawList* lights = ctx.m_pLights;
    if (lights != NULL && lights->count != 0) {
        const CMatrix* world = ctx.m_pWorld;
        CLight* best = NULL;
        float bestStrength = 0.0f;
        CVector3 objPos(world->m[3][0], world->m[3][1], world->m[3][2]);
        for (CLight** it = lights->items; it != &ctx.m_pLights->items[ctx.m_pLights->count]; it++) {
            CVector3 d;
            GetLightDirection(*it, objPos, d);
            float xx = d.x * d.x;
            float yy = d.y * d.y;
            float zz = d.z * d.z;
            float distSq = zz + (xx + yy);
            float radius = (*it)->m_radius;
            float radiusSq = radius * radius;
            if (distSq < radiusSq) {
                float strength = 1.0f - distSq / radiusSq;
                if (strength > bestStrength) {
                    bestStrength = strength;
                    best = *it;
                }
            }
        }
        if (best != NULL) {
            if (numDir >= 3) {
                numDir = 2;
            }
            CVector3 d;
            GetLightDirection(best, objPos, d);
            float xx = d.x * d.x;
            float yy = d.y * d.y;
            float zz = d.z * d.z;
            float len = sqrtf(zz + (xx + yy));
            if (len != 0.0f) {
                float inv = 1.0f / len;
                d.x = d.x * inv;
                d.y = d.y * inv;
                d.z = d.z * inv;
            }
            lighting.dir[numDir][0] = d.x;
            lighting.dir[numDir][1] = d.y;
            lighting.dir[numDir][2] = d.z;
            lighting.color[numDir][0] = bestStrength * best->m_color[0];
            lighting.color[numDir][1] = bestStrength * best->m_color[1];
            lighting.color[numDir][2] = bestStrength * best->m_color[2];
        }
        ctx.m_pNearestLight = best;
    }

    DMLightingUpdate(render, &lighting);
    if (m_animFlags & 0x100) {
        DMLightingScaleAlpha(render, (float)field_0xA3C / 127.0f);
    }
    g_matStack->Pop();
    m_pMesh->Draw(ctx, render);
}

void CAnimated::BindMesh(CDMesh* mesh) {
    DMRenderBindModel((DMRenderObject_T*)m_render, mesh);
    m_pMesh = mesh;
}

// Binds `texture` to part `part`, or to every part when `part` < 0.
void CAnimated::BindTexture(CTexture* texture, char part) {
    if (m_pMesh == NULL) {
        return;
    }
    if (part >= 0) {
        m_pMesh->m_parts[part].m_texture = texture;
        return;
    }
    for (u32 i = 0; i < m_pMesh->m_numParts; i++) {
        m_pMesh->m_parts[i].m_texture = texture;
    }
}

void CAnimated::BindSleeveTexture(CTexture* texture) {
    if (m_pMesh == NULL) {
        return;
    }
    for (u32 i = 0; i < m_pMesh->m_numParts; i++) {
        CDMeshPart* part = &m_pMesh->m_parts[i];
        if (part->m_flags & DMESHPART_SLEEVE) {
            part->m_texture = texture;
        }
    }
}

void CAnimated::SetGeomNodeState(long node, long state) {
    DMGeomSetNodeState((DMGeometryState_T*)m_geomState, node, state);
}

void CAnimated::AdjustRootTrans(CVector3 delta) {
    CharFrame_t* frame = m_pBaseFrame;
    frame->pos[0] = frame->pos[0] + delta.x;
    frame->pos[1] = frame->pos[1] + delta.y;
    frame->pos[2] = frame->pos[2] + delta.z;
}

void CAnimated::SetAnimSpeed(unsigned char channel, float speed) {
    AnimChanSetSpeed(&m_channels[channel], speed);
}

void CAnimated::BindSkeleton(CharSkel_t* skel) {
    DMClusterBindSkeleton((DMClusterObject_T*)&m_numClusterMatrices, skel);
    m_pSkel = skel;
}

CAnimMatrixCache::CAnimMatrixCache(int numSlots, int vectorsPerSlot, int matricesPerSlot) {
    m_numSlots = numSlots;
    m_vectorsPerSlot = vectorsPerSlot;
    m_matricesPerSlot = matricesPerSlot;
    m_buffer = 0;
    m_slots = (Slot*)DWI_alloc("Animation Matrix Cache", m_numSlots * sizeof(Slot), 0x400);
    for (int i = 0; i < m_numSlots; i++) {
        m_slots[i].key = NULL;
        m_slots[i].age = 0;
    }
    m_vectorBuffers[0] = NULL;
    m_vectorBuffers[1] = NULL;
    m_matrixBuffers[0] = (u8*)DWI_alloc("Animation Matrix buffer 0",
                                        m_matricesPerSlot * (m_numSlots * 0x30), 0x400);
    m_matrixBuffers[1] = NULL;
}

// Ages every used slot.
void CAnimMatrixCache::Update() {
    m_buffer = 0;
    for (int i = 0; i < m_numSlots; i++) {
        if (m_slots[i].key != NULL) {
            m_slots[i].age++;
        }
    }
}

CAnimMatrixCacheEntry CAnimMatrixCache::GetEntry(void* key, bool& isNew) {
    CAnimMatrixCacheEntry entry;
    int victim = 0;
    int victimAge = 0;
    isNew = false;
    for (int i = 0; i < m_numSlots; i++) {
        Slot* slot = &m_slots[i];
        if (slot->key == key) {
            m_slots[i].age = 0;
            entry.vectors = m_vectorBuffers[m_buffer] + i * m_vectorsPerSlot * 0x10;
            entry.matrices = m_matrixBuffers[m_buffer] + i * m_matricesPerSlot * 0x30;
            return entry;
        }
        if (slot->key == NULL) {
            victim = i;
            victimAge = 0x7FFFFFF;
        } else if (slot->age > victimAge) {
            victim = i;
            victimAge = slot->age;
        }
    }
    isNew = true;
    m_slots[victim].key = key;
    m_slots[victim].age = 0;
    entry.vectors = m_vectorBuffers[m_buffer] + victim * m_vectorsPerSlot * 0x10;
    entry.matrices = m_matrixBuffers[m_buffer] + victim * m_matricesPerSlot * 0x30;
    return entry;
}
