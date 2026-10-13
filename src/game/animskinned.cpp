#include "game/animskinned.h"

#include "game/anim.h"
#include "game/animmot.h"
#include "game/animst.h"
#include "game/dmcluster.h"
#include "game/dmgeom.h"
#include "game/dmrender.h"
#include "game/matrix.h"
#include "game/matstack.h"
#include "game/skelblend.h"

static CAnimMatrixCache g_animSkinnedCache(0x10, 0x180, 0xC0);

CAnimSkinned::CAnimSkinned() {
    m_frame.numJoints = 0x50;
    m_frame.jointData = &m_jointData[0][0];
    m_pBaseFrame = &m_frame;
}

CAnimSkinned::~CAnimSkinned() {}

void CAnimSkinned::InitClass() {}

void CAnimSkinned::Draw(CDrawContext& ctx) {
    g_animSkinnedCache.Update();
    bool isNew;
    CAnimMatrixCacheEntry entry = g_animSkinnedCache.GetEntry(this, isNew);
    m_clusterMatrices = entry.matrices;
    m_numClusterMatrices = 0xC0;
    if (isNew) {
        DMRenderSetFlags((DMRenderObject_T*)m_render, 1);
    }
    CAnimated::Draw(ctx);
}

void CAnimSkinned::Init(CAnimObject* owner, unsigned short arg, CharFrame_t* baseFrame) {
    AnimInitHeader((u16*)&m_hdr, 0, arg, 0x10);
    CAnimated::Init(owner, arg);
    DMRenderInitObject((DMRenderObject_T*)m_render, NULL, NULL, 0);
    DMClusterInitObject((DMClusterObject_T*)&m_numClusterMatrices, NULL, NULL, 0);
    DMRenderBindClusterObj((DMRenderObject_T*)m_render, (DMClusterObject_T*)&m_numClusterMatrices);
    DMGeomSetNodeState((DMGeometryState_T*)m_geomState, 0, 0);
    if (baseFrame != NULL) {
        m_pBaseFrame = baseFrame;
    }
}

// Note: the rotation flag is set (not or'ed) and never cleared, as in the original.
void CAnimSkinned::UpdateAnimation(float dt) {
    CAnimated::UpdateAnimation(dt);
    BlendSkelFrames(&m_frame, m_pFrame);
    if (!field_0xCC1) {
        field_0xCC0 = m_pFrame->frames[0].flags >> 7;
    }
    u16 state = AnimStGetCurrentState(m_stateInfo);
    m_motionRot = 0;
    if (AnimMotCalcAnimRot(&m_hdr, m_channels, m_pFrame, &m_motionRot, state, 0)) {
        m_motionFlags = ANIMSKINNED_HAS_ROTATION;
    }
    m_motionTrans.z = m_motionTrans.y = m_motionTrans.x = 0.0f;
    if (AnimMotCalcAnimPos(&m_hdr, m_channels, m_pFrame, &m_motionTrans, state, 0)) {
        m_motionFlags |= ANIMSKINNED_HAS_TRANSLATION;
    }
    AnimMotCalcFloatingBone(&m_hdr, m_channels, m_pFrame, &m_pBaseFrame->floatPos,
                            &m_pBaseFrame->floatRot, 0xFFFF, 1);
}

int CAnimSkinned::ExtractRotation(long* out) {
    if (m_motionFlags & ANIMSKINNED_HAS_ROTATION) {
        *out = m_motionRot;
        return 1;
    }
    return 0;
}

int CAnimSkinned::ExtractTranslation(CVector3* out) {
    if (m_motionFlags & ANIMSKINNED_HAS_TRANSLATION) {
        *out = m_motionTrans;
        return 1;
    }
    return 0;
}

void CAnimSkinned::SetRotation(long rot) {
    m_hdr.field_0x8 = rot;
}

// Joint offsets are stored in 1/16 units in the pose.
void CAnimSkinned::BindSkeleton(CharSkel_t* skel) {
    m_frame.rot = 0;
    m_frame.pos[0] = 0.0f;
    m_frame.pos[1] = 0.0f;
    m_frame.pos[2] = 0.0f;
    for (u32 i = 0; i < skel->numJoints; i++) {
        m_frame.jointData[i * 3 + 0] = skel->jointOffsets[i][0] << 4;
        m_frame.jointData[i * 3 + 1] = skel->jointOffsets[i][1] << 4;
        m_frame.jointData[i * 3 + 2] = skel->jointOffsets[i][2] << 4;
    }
    DMClusterBindSkeleton((DMClusterObject_T*)&m_numClusterMatrices, skel);
    m_pSkel = skel;
}

void CAnimSkinned::UpdateSkeletonMatrices(CMatrix* matrices, CVector3* min, CVector3* max) {
    g_matStack->PushUnit();
    DMClusterSkelTraverse(&m_frame, m_pSkel, matrices, min, max);
    g_matStack->Pop();
    if (min != NULL) {
        min->x = min->x - 0.1f;
        min->y = min->y - 0.1f;
        min->z = min->z - 0.1f;
    }
    if (max != NULL) {
        max->x = max->x + 0.1f;
        max->y = max->y + 0.1f;
        max->z = max->z + 0.1f;
    }
}
