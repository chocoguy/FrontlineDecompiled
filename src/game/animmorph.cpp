// CAnimMorph: an animated mesh driven by morph-target weights. Key frames
// carry packed weights; channels are blended into m_weights each update.
#include "game/animmorph.h"
#include "game/anim.h"
#include "game/animmot.h"
#include "game/dmcluster.h"
#include "game/dmgeom.h"
#include "game/dmmorph.h"
#include "game/dmrender.h"
#include "game/draw_context.h"
#include "game/endian.h"
#include "game/matrix.h"

static CAnimMatrixCache g_animMorphCache(0x10, 0x180, 8);

CAnimMorph::CAnimMorph() {}

CAnimMorph::~CAnimMorph() {}

// Re-blends the morph display lists only when the object is big enough on
// screen: a 2-unit span at its position must cover over 1.5 (squared)
// projected units.
void CAnimMorph::Draw(CDrawContext& ctx) {
    if (m_pMesh == NULL) {
        return;
    }
    g_animMorphCache.Update();
    bool isNew;
    CAnimMatrixCacheEntry entry = g_animMorphCache.GetEntry(this, isNew);
    m_clusterMatrices = entry.matrices;
    m_numClusterMatrices = 8;
    if (isNew) {
        DMRenderSetFlags((DMRenderObject_T*)m_render, 1);
    }

    const CMatrix* world = ctx.m_pWorld;
    CVector3 pos(world->m[3][0], world->m[3][1], world->m[3][2]);
    CVector3 viewPos;
    ctx.m_pView->TransformPoint(viewPos, pos);
    CVector3 left = viewPos;
    left.x = left.x - 1.0f;
    CVector3 right = viewPos;
    right.x = right.x + 1.0f;
    CVector4 a;
    ctx.m_pProj->Transform(a, left);
    CVector4 b;
    ctx.m_pProj->Transform(b, right);
    a.y = a.y / a.w;
    b.y = b.y / b.w;
    a.x = a.x / a.w;
    b.x = b.x / b.w;
    float dy = b.y - a.y;
    float dx = b.x - a.x;
    if (dx * dx + dy * dy > 1.5f) {
        DMMorphBlendDisplayLists((DMRenderObject_T*)m_render);
    }
    CAnimated::Draw(ctx);
}

void CAnimMorph::Init(CAnimObject* owner, unsigned short arg, CharFrame_t* baseFrame) {
    AnimInitHeader((u16*)&m_hdr, 2, arg, 4);
    CAnimated::Init(owner, arg);
    m_pBaseFrame = baseFrame;
    DMRenderInitObject((DMRenderObject_T*)m_render, NULL, NULL, 0);
    DMClusterInitObject((DMClusterObject_T*)&m_numClusterMatrices, NULL, NULL, 0);
    DMRenderBindClusterObj((DMRenderObject_T*)m_render, (DMClusterObject_T*)&m_numClusterMatrices);
    DMMorphInitObject((DMMorphObject_T*)m_weights, ANIMMORPH_NUM_WEIGHTS);
    DMRenderBindMorphObj((DMRenderObject_T*)m_render, (DMMorphObject_T*)m_weights);
    DMMorphSetMaxActiveTargets((DMMorphObject_T*)m_weights, 12);
    DMGeomSetNodeState((DMGeometryState_T*)m_geomState, 0, 0);
    m_weightsDirty = true;
}

void CAnimMorph::SetBlendTargetWeight(unsigned long target, float weight) {
    bool same = weight == m_weights[target];
    m_weights[target] = weight;
    if (!same) {
        DMRenderSetFlags((DMRenderObject_T*)m_render, 1);
    }
}

// Unpacks a key frame's weights: a count, then entries of a 6-bit target
// (stored as target + 1) and a 10-bit value mapping 0..1023 to -0.5..1.5.
static inline void DecodeMorphFrame(float* weights, u8* frame) {
    u16* p = (u16*)frame;
    s16 count = *p;
    EndianSwap(count);
    for (u16 i = 0; i < (u16)count; i++) {
        s16 entry = p[i + 1];
        EndianSwap(entry);
        u32 value = ((u32)entry >> 6) & 0x3FF;
        weights[(entry & 0x3F) + 1] = 2.0f * ((float)value * 0.0009765625f) - 0.5f;
    }
}

static inline void ClearWeights(float* weights) {
    for (int i = 0; i < ANIMMORPH_NUM_WEIGHTS; i++) {
        weights[i] = 0.0f;
    }
}

// Blends the weights of every key frame in `list` into `weights`.
static void LLCharObjCompositeMorph(float* weights, AnimMotFrameList_t* list) {
    u16 count = list->count;
    if (count == 0) {
        return;
    }
    ClearWeights(weights);
    DecodeMorphFrame(weights, list->frames[0].frame);
    for (u32 i = 1; i < count; i++) {
        float frameWeights[ANIMMORPH_NUM_WEIGHTS];
        ClearWeights(frameWeights);
        DecodeMorphFrame(frameWeights, list->frames[i].frame);
        float blend = list->frames[i].blend;
        for (int j = 0; j < ANIMMORPH_NUM_WEIGHTS; j++) {
            weights[j] = blend * (frameWeights[j] - weights[j]) + weights[j];
        }
    }
}

void CAnimMorph::UpdateAnimation(float dt) {
    CAnimated::UpdateAnimation(dt);
    if (m_pFrame->count != 0) {
        LLCharObjCompositeMorph(m_weights, m_pFrame);
        DMRenderSetFlags((DMRenderObject_T*)m_render, 1);
        m_weightsDirty = true;
    } else if (m_weightsDirty) {
        ClearWeights(m_weights);
        m_weightsDirty = false;
    }
}
