// CProjectileBullet: straight-flying rounds and their tracers.
#include "game/projectilebullet.h"
#ifdef FRONTLINE_NATIVE
#include "native/ai.h"
#endif
#include "game/bulletfactory.h"
#include "game/collision.h"
#include "game/SClsnContact.h"
#include "game/IVolume.h"
#include "game/line.h"
#include "game/matrix.h"
#include "game/scene.h"
#include "game/player.h"
#include "game/hearingvolume.h"
#include "game/draw_context.h"
#include "game/renderlist.h"
#include "game/particlesystem.h"
#include "game/toilet.h"
#include "game/ShellMenu.h"
#include "game/Moh2.h"
#include "game/MathFun.h"
#include "dolphin/gx.h"

#define BIG_FLOAT 3.4028235e38f

float MP_PROJBULLET_DRAW_DELAY = 1.0f;

static CMatrix g_WorldToCamera;
static CProjectileBulletRenderBin g_BulletBin;
static CVector3 g_XAxis;  // the camera's rightward and upward axes
static CVector3 g_YAxis;
static CRenderList* g_pRenderList;
static CScreen* g_pScreen;
static CDrawContext* g_pContext;
// Like the original, never cleared once set, so the tracers keep facing
// the first camera axes the bin cached.
static bool g_bCached;

// What Draw hands the render list for one tracer.
struct STracerDraw {
    /* 0x00 */ CVector3 m_start;
    /* 0x10 */ u8 unk_0x10[0x10];
    /* 0x20 */ CVector3 m_end;
    /* 0x30 */ u8 unk_0x30[0x10];
    /* 0x40 */ f32 m_width;
};

void CProjectileBulletRenderBin::Link(CDmaTag* tag, CDmaPacket& packet) {
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);   // GX_VA_POS, direct
    GXSetVtxDesc(11, 1);  // GX_VA_CLR0, direct
    GXSetVtxDesc(13, 1);  // GX_VA_TEX0, direct
    GXSetVtxAttrFmt(0, 9, 1, 4, 0);   // xyz f32
    GXSetVtxAttrFmt(0, 11, 1, 5, 0);  // rgba8
    GXSetVtxAttrFmt(0, 13, 1, 4, 0);  // st f32
    GXSetNumChans(1);
    GXSetNumTexGens(1);
    GXSetTexCoordGen2(0, 1, 4, 0x3C, 0, 0x7D);
    GXSetNumTevStages(1);
    GXSetBlendMode(1, 4, 5, 5);
    GXSetZMode(1, 3, 0);  // test, no write
    GXSetCullMode(0);
    GXSetTevOrder(0, 0, 0, 4);
    GXSetTevColorIn(0, 0xF, 8, 0xA, 0xF);
    GXSetTevColorOp(0, 0, 0, 1, 1, 0);
    GXSetTevAlphaIn(0, 7, 4, 5, 7);
    GXSetTevAlphaOp(0, 0, 0, 2, 1, 0);
    GXSetChanCtrl(4, 0, 0, 1, 0, 0, 2);
    m_texture.Load(packet, NULL);
    for (; tag != NULL; tag = tag->m_next) {
        CVector3 verts[4];
        ((STracer*)tag->m_data)->GetVerts(verts);
        f32 mtx[3][4];
        g_WorldToCamera.GetGAMECUBEMatrix34(mtx);
        GXLoadPosMtxImm(mtx, 0);
        GXSetCurrentMtx(0);
        GXBegin(0x80, 0, 4);  // GX_QUADS
        GXPosition3f32(verts[0].x, verts[0].y, verts[0].z);
        GXColor4u8(0xFF, 0xFF, 0xFF, 0xFF);
        GXTexCoord2f32(0.0f, 0.0f);
        GXPosition3f32(verts[1].x, verts[1].y, verts[1].z);
        GXColor4u8(0xFF, 0xFF, 0xFF, 0xFF);
        GXTexCoord2f32(0.0f, 1.0f);
        GXPosition3f32(verts[2].x, verts[2].y, verts[2].z);
        GXColor4u8(0xFF, 0xFF, 0xFF, 0xFF);
        GXTexCoord2f32(1.0f, 1.0f);
        GXPosition3f32(verts[3].x, verts[3].y, verts[3].z);
        GXColor4u8(0xFF, 0xFF, 0xFF, 0xFF);
        GXTexCoord2f32(1.0f, 0.0f);
        GXEnd();
    }
    GXSetZMode(1, 3, 1);
    g_bCached = true;
}

// The quad spans the segment, widened along the camera's diagonal that is
// most across it.
void STracer::GetVerts(CVector3* verts) {
    float dx = m_end[0] - m_start[0];
    float dy = m_end[1] - m_start[1];
    float dz = m_end[2] - m_start[2];
    CVector3 side;
    if ((dx * g_XAxis.x + dy * g_XAxis.y + dz * g_XAxis.z) * (dx * g_YAxis.x + dy * g_YAxis.y + dz * g_YAxis.z) > 0.0f) {
        side.x = g_YAxis.x - g_XAxis.x;
        side.y = g_YAxis.y - g_XAxis.y;
        side.z = g_YAxis.z - g_XAxis.z;
    } else {
        side.x = -1.0f * g_YAxis.x - g_XAxis.x;
        side.y = -1.0f * g_YAxis.y - g_XAxis.y;
        side.z = -1.0f * g_YAxis.z - g_XAxis.z;
    }
    float scale;
    if (g_bInMultiplayerMode) {
        scale = 0.05f;
    } else {
        scale = 0.025f;
    }
    float width = m_width * scale;
    side.Normalize();
    side.x *= width;
    side.y *= width;
    side.z *= width;
    verts[0].x = m_start[0] + side.x;
    verts[0].y = m_start[1] + side.y;
    verts[0].z = m_start[2] + side.z;
    verts[1].x = m_start[0] - side.x;
    verts[1].y = m_start[1] - side.y;
    verts[1].z = m_start[2] - side.z;
    verts[2].x = m_end[0] - side.x;
    verts[2].y = m_end[1] - side.y;
    verts[2].z = m_end[2] - side.z;
    verts[3].x = m_end[0] + side.x;
    verts[3].y = m_end[1] + side.y;
    verts[3].z = m_end[2] + side.z;
}

int CProjectileBulletRenderBin::Render(CDmaPacket& packet, void* data) {
    if (!g_bCached) {
        g_bCached = true;
        g_pContext->m_pCamera->GetRightward(g_XAxis);
        g_pContext->m_pCamera->GetUpward(g_YAxis);
    }
    STracerDraw* draw = (STracerDraw*)data;
    CVector3 start(draw->m_start);
    CVector3 end(draw->m_end);
    float width = draw->m_width;
    g_WorldToCamera = *g_pContext->m_pView;
    STracer* tracer = (STracer*)packet.m_ptr;
    *(CVector3*)tracer->m_start = start;
    *(CVector3*)tracer->m_end = end;
    tracer->m_width = width;
    packet.m_ptr += sizeof(STracer);
    return 0;
}

void CProjectileBullet::GetVelocity(CVector3& v) {
    CVector3 forward(m_forward);
    v.x = m_speed * forward.x;
    v.y = m_speed * forward.y;
    v.z = m_speed * forward.z;
}

// Only one node is excluded at a time.
void CProjectileBullet::SetExclusionPair(ISceneNode* node) {
    if (m_pExcluded != NULL) {
        g_scene.IncludeCollision(*m_pExcluded, *this);
    }
    m_pExcluded = node;
}

int CProjectileBullet::GetScriptBulletType() const {
    switch (m_type) {
    case 0:
    case 1:
        return 0xA;
    case 2:
    case 3:
        return 0;
    case 4:
    case 5:
        return 4;
    case 6:
        return 0xC;
    case 7:
    case 8:
        return 0xD;
    case 9:
        return 0xB;
    case 10:
    case 13:
        return 5;
    case 11:
        return 0xE;
    case 12:
        return 0xF;
    case 14:
    case 15:
        return 6;
    case 16:
    case 17:
        return 1;
    case 18:
    case 19:
        return 0x10;
    case 20:
    case 21:
        return 0x13;
    case 22:
    case 23:
        return 0x14;
    case 24:
    case 25:
        return 2;
    case 26:
    case 27:
        return 0x12;
    case 28:
        return 0x11;
    case 29:
        return 0x1B;
    default:
        DebugMsg("WARNING: Unknown proj bullet type in GetScriptBulletType.  Type %d\n", m_type);
        return 4;
    }
}

int CProjectileBullet::IsVisible(CDrawContext& ctx) const {
    return m_worldFiber.TestVisibility(ctx);
}

// Queues the tracer: from a random point back along this frame's sweep,
// as long as the velocity scaled by the tracer width. In multiplayer, other
// players' young bullets aren't drawn (they would start in their face).
void CProjectileBullet::Draw(CDrawContext& ctx) {
    if (g_cheats.field_0xB) {
        return;
    }
    if (g_bInMultiplayerMode && m_pFiredBy != NULL && m_pFiredBy->AsPlayerObject() != NULL &&
        ctx.field_0x114 != (int)m_pFiredBy->AsPlayerObject()->field_0x768 &&
        field_0x90 > (float)m_pProps->m_lifetime - MP_PROJBULLET_DRAW_DELAY) {
        return;
    }
    if (m_pProps->m_tracerWidth == 0.0f) {
        return;
    }
    CVector3 length;
    GetVelocity(length);
    float scale = 0.09f * (m_pProps->m_tracerWidth - 0.5f) + 0.455f;
    length.x *= scale;
    length.y *= scale;
    length.z *= scale;
    float offset = m_tracerOffset;
    CVector3 dir(m_worldFiber.GetLine().m_dir);
    STracerDraw draw;
    draw.m_start.x = m_prevPos.x + offset * dir.x;
    draw.m_start.y = m_prevPos.y + offset * dir.y;
    draw.m_start.z = m_prevPos.z + offset * dir.z;
    draw.m_end.x = draw.m_start.x + length.x;
    draw.m_end.y = draw.m_start.y + length.y;
    draw.m_end.z = draw.m_start.z + length.z;
    draw.m_width = m_pProps->m_tracerWidth;
    g_pContext = &ctx;
    g_pRenderList->Render(g_BulletBin, &draw);
}

int CProjectileBullet::GetCollisionId() const {
    return m_collisionId;
}

// Passing through (shoot-through surfaces): step just past the contact
// along the bullet's direction.
int CProjectileBullet::Penetrate(const CCollision& clsn) {
    SClsnContact* contact = clsn.m_pContacts;
    CVector3 step;
    GetForward(step);
    float along = step.x * contact->m_normal.x + step.y * contact->m_normal.y + step.z * contact->m_normal.z;
    float absAlong = (float)fabs(along);
    if (absAlong > 0.00001f) {
        float scale = CONTACT_EPSILON / absAlong;
        step.x *= scale;
        step.y *= scale;
        step.z *= scale;
    } else if (along > 0.0f) {
        step.x = CONTACT_EPSILON * contact->m_normal.x;
        step.y = CONTACT_EPSILON * contact->m_normal.y;
        step.z = CONTACT_EPSILON * contact->m_normal.z;
    } else {
        float eps = -CONTACT_EPSILON;
        step.x = eps * contact->m_normal.x;
        step.y = eps * contact->m_normal.y;
        step.z = eps * contact->m_normal.z;
    }
    CVector3 pos(m_position);
    CVector3 next(pos.x + step.x, pos.y + step.y, pos.z + step.z);
    ((CMatrix*)&m_rightward)->SetPos(next);
    m_flags &= ~FLAG_HIT;
    END_RETURN_NOTHING
}

// Dies when its time runs out or it hit something.
int CProjectileBullet::CommitUpdate() {
    if (field_0x90 <= 0.0f || (m_flags & FLAG_HIT)) {
        MarkForDestruction(1);
        RETURN_NOTHING;
    }
    m_basePos = m_position;
    END_RETURN_NOTHING
}

// Moves speed * dt along its forward axis, sweeping a fiber from the old
// position, and updates the scene's extents.
int CProjectileBullet::AttemptUpdate(float dt) {
    field_0x94 = BIG_FLOAT;
    CVector3 forward(m_forward);
    float step = m_speed * dt;
    CVector3 pos(m_basePos.x + step * forward.x, m_basePos.y + step * forward.y, m_basePos.z + step * forward.z);
    ((CMatrix*)&m_rightward)->SetPos(pos);
    m_localFiber.Set(CVector3(0.0f, -step, 0.0f), CVector3(0.0f, 0.0f, 0.0f));
    m_worldFiber.TransformedCopy(m_localFiber, *(CMatrix*)&m_rightward);
    CVector3 min;
    CVector3 max;
    m_worldFiber.GetExtents(min, max);
    m_pMinX->m_value = min.x - CONTACT_EPSILON;
    m_pMinY->m_value = min.y - CONTACT_EPSILON;
    m_pMaxX->m_value = CONTACT_EPSILON + max.x;
    m_pMaxY->m_value = CONTACT_EPSILON + max.y;
    END_RETURN_NOTHING
}

int CProjectileBullet::BeginUpdate(float dt) {
    m_basePos = m_position;
    m_prevPos = m_basePos;
#ifdef FRONTLINE_NATIVE
    if (NativeAIOn()) {
        NativeAIBulletFlying(this, dt);
    }
#endif
    field_0x94 = BIG_FLOAT;
    if (field_0x90 > 0.0f) {
        field_0x90 -= dt;
        if (field_0x90 < 0.0f) {
            field_0x90 = 0.0f;
        }
    }
    END_RETURN_NOTHING
}

void CProjectileBullet::OnCollision(const CCollision& clsn) {
    ISceneNode* other = clsn.m_pNode0;
    if (other == this) {
        other = clsn.m_pNode1;
    }
    m_flags |= FLAG_HIT;
    other->HandleBulletCollision(this, clsn);
}

void CProjectileBullet::Shutdown() {
    m_flags |= FLAG_SHUTDOWN;
    CBullet::Shutdown();
}

void CProjectileBullet::Destroy() {
    g_pBulletFactory->DestroyBullet(this);
}

void CProjectileBullet::MarkForDestruction(int when) {
    ISubject::MarkForDestruction(when);
    m_flags |= FLAG_HIT;
    g_scene.Remove(*this);
    m_collisionId = -1;
    if (m_pTrail != NULL) {
        m_pTrail->DeActivate();
        m_pTrail = NULL;
    }
}

// Fires the round from `pos` along `dir`. The basis keeps the bullet's up
// axis near world up (or near world x for steep shots). A round fired from
// inside the player's hearing volume doesn't collide with it.
void CProjectileBullet::Init(EProjectileBulletTypes type, ProjectileBulletProperties_struct* props,
                             const CVector3& pos, const CVector3& dir, float speed, EClsnId clsnId, float,
                             ISceneNode* firedBy) {
    CBullet::Init();
    m_type = type;
    m_pProps = props;
    field_0x90 = (float)props->m_lifetime;
    m_speed = speed * m_pProps->m_weight;
    if (g_bInMultiplayerMode) {
        m_tracerOffset = MathFunRandomReal(0.2f, 0.5f);
    } else {
        m_tracerOffset = MathFunRandomReal(0.0f, 0.5f);
    }
    m_basePos = pos;
    m_prevPos = pos;
    m_collisionId = clsnId;
    m_pFiredBy = firedBy;
    m_pExcluded = NULL;
    m_flags &= ~FLAG_HIT;
    m_flags &= ~FLAG_SHUTDOWN;
    m_pTrail = NULL;
    CMatrix& tm = *(CMatrix*)&m_rightward;
    tm.BuildTrans(pos);

    CVector3 front(dir);
    front.Normalize();
    CVector3 right;
    CVector3 up;
    if ((float)fabs(front.z) < (float)fabs(front.x)) {
        CVector3 z(0.0f, 0.0f, 1.0f);
        up = z;
        right.x = front.y * z.z - front.z * z.y;
        right.y = front.z * z.x - front.x * z.z;
        right.z = front.x * z.y - front.y * z.x;
        up.x = right.y * front.z - right.z * front.y;
        up.y = right.z * front.x - right.x * front.z;
        up.z = right.x * front.y - right.y * front.x;
    } else {
        if (front.y < 0.0f) {
            right.y = 0.0f;
            right.x = -1.0f;
            right.z = 0.0f;
        } else {
            right.y = 0.0f;
            right.x = 1.0f;
            right.z = 0.0f;
        }
        up.x = right.y * front.z - right.z * front.y;
        up.y = right.z * front.x - right.x * front.z;
        up.z = right.x * front.y - right.y * front.x;
        right.z = front.x * up.y - front.y * up.x;
        right.x = front.y * up.z - front.z * up.y;
        right.y = front.z * up.x - front.x * up.z;
    }
    tm.SetFront(front);
    tm.SetRight(right);
    tm.SetUp(up);
    g_scene.Add(*this);

    m_localFiber.Set(CVector3(0.0f, 0.0f, 0.0f), CVector3(0.0f, 0.0f, 0.0f));
    CPlayerObject* player = g_scene.GetPlayer(0);
    m_worldFiber.TransformedCopy(m_localFiber, tm);
    ISceneNode& hearing = player->m_hearing;
    const IVolume* volume = hearing.GetWorldBoundingVolume((ISceneNode::EVolumeType)0);
    CCollision clsn;
    if (volume->TestCollision(m_worldFiber, clsn, false) != CLSN_NONE) {
        g_scene.ExcludeCollision(*this, hearing, -1);
    }

    // Bullet-cam cheat: slow rounds with a smoke trail.
    if (g_cheats.field_0xB) {
        float slow;
        if (g_Shell.Get_currentMission() == 6 && g_Shell.Get_currentStage() == 1 && !g_bInMultiplayerMode) {
            slow = 0.85f;
        } else {
            slow = 0.3f;
        }
        m_pTrail = CParticleSystem::Create(0x6CDC8936);
        if (m_pTrail != NULL) {
            m_speed *= slow;
            CVector3 v(m_forward);
            float s = 60.0f * m_speed;
            v.x *= s;
            v.y *= s;
            v.z *= s;
            m_pTrail->SetSystemInitialVelocity(v);
            m_pTrail->SetLocalToWorld(tm);
        }
    }

    if (!g_BulletBin.m_loaded) {
        g_BulletBin.m_loaded = true;
        LevelFileContentsStruct_* res = TLT_FindNextResourceByType((TLTResourceID)0xD, NULL);
        g_BulletBin.m_texture.Set((ShapeFile*)TLT_LoadFileFromLevelBigFile(ADDR_TO_PTR(const char, res->fields[0].value), NULL), false);
    }
}

void CProjectileBullet::InitClass(CRenderList* list, CScreen* screen) {
    g_pScreen = screen;
    g_pRenderList = list;
    list->Register(g_BulletBin);
}

CProjectileBullet::~CProjectileBullet() {}

CProjectileBullet::CProjectileBullet() {
    field_0x28 = 1;
}

void* CProjectileBullet::operator new[](unsigned long size) {
    return DWI_alloc(NULL, size, 0x400);
}

int CProjectileBullet::Init() {
    return CBullet::Init();
}

const IVolume* CProjectileBullet::GetLocalBoundingVolume(ISceneNode::EVolumeType) const {
    return &m_localFiber;
}

const IVolume* CProjectileBullet::GetWorldBoundingVolume(ISceneNode::EVolumeType) const {
    return &m_worldFiber;
}

float CProjectileBullet::GetDamage() const {
    return m_pProps->m_damage;
}

ISceneNode* CProjectileBullet::GetFiredBy() const {
    return m_pFiredBy;
}

CProjectileBullet* CProjectileBullet::AsProjectile() {
    return this;
}
