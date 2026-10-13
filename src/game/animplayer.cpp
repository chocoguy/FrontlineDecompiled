// A player's third-person body, seen by the other players in multiplayer.
// The player object moves it, so it has no AI, gravity or ground
// detection; it shows the held weapon (muzzle flash, rocket launchers
// emptied while reloading), isn't drawn in its own player's view, and
// passes bullet hits on to the player (the player's own shots go through).
#include "game/animplayer.h"
#include "game/player.h"
#include "game/weapon.h"
#include "game/bullet.h"
#include "game/collision.h"
#include "game/SClsnContact.h"
#include "game/draw_context.h"
#include "game/animated_volume.h"
#include "game/box.h"
#include "game/objcreate.h"
#include "game/particlesystem.h"
#include "game/scene.h"
#include "game/ShellMenu.h"
#include "game/MathFun.h"
#include "math.h"
#include "string.h"
#include "stdlib.h"

void CAnimatedPlayer::UpdateAI(float) {}

const CAnimatedPlayer* CAnimatedPlayer::AsAnimatedPlayerObject() const {
    return this;
}

CAnimatedPlayer* CAnimatedPlayer::AsAnimatedPlayerObject() {
    return this;
}

// CAnimObject::CommitUpdate without the ground detection.
int CAnimatedPlayer::CommitUpdate() {
    ConstrainVelocity();
    CVector3 pos = *(const CVector3*)m_tm.m[3];
    m_turnFrom[0] = pos.x;
    m_turnFrom[1] = pos.y;
    m_turnFrom[2] = pos.z;
    m_turnFrom[3] = pos.w;
    m_lastHeading = m_heading;
    m_lastRootPos.Copy(m_rootPos);
    m_lastRootRot = m_rootRot;
}

int CAnimatedPlayer::AttemptUpdate(float dt) {
    CAnimObject::AttemptUpdate(dt);
    CWeapon* held = m_pPlayer->GetCurrentWeapon();
    if (m_pPlayer->m_dead) {
        m_pWeapons[m_curWeapon]->m_muzzleFlash = 0;
        m_pWeapons[m_curWeapon]->m_drawEnabled = 0;
        return;
    }
    m_pWeapons[m_curWeapon]->m_muzzleFlash = held->GetMuzzleFlashStatus();
    // A rocket launcher is shown empty while it reloads.
    if (held->m_refireTime > 60.0f &&
        (m_pWeapons[m_curWeapon]->m_weaponType == 0x21 || m_pWeapons[m_curWeapon]->m_weaponType == 0x22)) {
        m_pWeapons[m_curWeapon]->m_drawEnabled = 0;
        return;
    }
    m_pWeapons[m_curWeapon]->m_drawEnabled = 1;
}

// CAnimObject::BeginUpdate without gravity: the player sets the position.
int CAnimatedPlayer::BeginUpdate(float dt) {
    m_lightVolumes.Update(dt);
    CVector3 pos = *(const CVector3*)m_tm.m[3];
    m_turnFrom[0] = pos.x;
    m_turnFrom[1] = pos.y;
    m_turnFrom[2] = pos.z;
    m_turnFrom[3] = pos.w;
    m_lastHeading = m_heading;
    CMatrix root;
    root = m_bones[1];
    m_lastRootRot.SetFromMatrix(root);
    m_lastRootPos = *(const CVector3*)root.m[3];
    UpdateAnimation(dt);
    m_velocity.z = 0.0f;
    m_velocity.y = 0.0f;
    m_velocity.x = 0.0f;
    m_constrained = 0;
}

void CAnimatedPlayer::UpdateLocalBoundingVolume(ISceneNode::EVolumeType type, const CVector3& min,
                                                const CVector3& max) {
    switch ((int)type) {
    case 3:
        ((CVolBox*)m_pLocalBound)->SetCorners(min, max);
        break;
    }
}

// No hit spheres (types 1 and 2), unlike other soldiers.
const IVolume* CAnimatedPlayer::GetWorldBoundingVolume(ISceneNode::EVolumeType type) const {
    switch ((int)type) {
    case 3:
        return m_pWorldBound;
    case 0:
    case 1:
    case 2:
        return m_pWorldVolume;
    default:
        return NULL;
    }
}

const IVolume* CAnimatedPlayer::GetLocalBoundingVolume(ISceneNode::EVolumeType type) const {
    switch ((int)type) {
    case 3:
        return m_pLocalBound;
    case 0:
    case 1:
    case 2:
        return m_pLocalVolume;
    default:
        return NULL;
    }
}

void CAnimatedPlayer::GetFlocoParams(float& time, long& state) {
    time = m_flocoTime;
    state = m_flocoState;
}

void CAnimatedPlayer::SetFlocoParams(float time, long state) {
    m_flocoTime = time;
    m_flocoState = state;
}

// The body animation (pitched by the player's aim) and the face, as
// CSoldierObject without the head tracking.
void CAnimatedPlayer::UpdateAnimation(float dt) {
    CAnimObject::UpdateAnimation(dt);
    field_0x30D8 = m_pPlayer->m_pitch;
    if (m_faceStateQueued) {
        m_faceStateQueued = 0;
        if (m_face.ValidState(m_faceState)) {
            u16 state;
            m_face.SetAnimationState(m_faceState, 1.0f, state, -1, NULL);
        }
    }
    m_face.UpdateAnimation(dt);
    if (m_moodTarget != -1) {
        m_face.SetBlendTargetWeight(m_moodTarget + 0x1C, 1.0f);
    }
    if (m_expression >= 0) {
        m_face.SetBlendTargetWeight(m_expression + 0xB, m_expressionWeight);
        if (m_nextExpression >= 0) {
            m_face.SetBlendTargetWeight(m_nextExpression + 0xB, m_nextExpressionWeight);
        }
        if (m_expressionWeight == 0.0f && m_nextExpression >= 0) {
            m_expression = m_nextExpression;
            m_expressionWeight = m_nextExpressionWeight;
            m_expressionRate = 0.0f;
            m_expressionTarget = m_nextExpressionTarget;
            m_nextExpression = -1;
        }
        m_expressionWeight += m_expressionRate;
        if (m_expressionWeight < 0.0f) {
            m_expressionWeight = 0.0f;
            m_expressionRate = 0.0f;
        } else if (m_expressionWeight > m_expressionTarget) {
            m_expressionWeight = m_expressionTarget;
            m_expressionRate = 0.0f;
        }
        m_nextExpressionWeight += m_nextExpressionRate;
        if (m_nextExpressionWeight < 0.0f) {
            m_nextExpressionWeight = 0.0f;
            m_nextExpressionRate = 0.0f;
        } else if (m_nextExpressionWeight > m_nextExpressionTarget) {
            m_nextExpressionWeight = m_nextExpressionTarget;
            m_nextExpressionRate = 0.0f;
        }
    }
    m_blinkTimer -= dt / 59.94f;
    if (m_blinkTimer <= 0.0f) {
        m_blinkOpenEnd = m_blinkOpen;
        m_blinkClosedEnd = m_blinkOpenEnd + m_blinkClose;
        m_blinkCloseEnd = m_blinkClosedEnd + m_blinkHold;
        m_blinkTimer = m_blinkCloseEnd + MathFunRandomReal(m_blinkMin, m_blinkMax);
    }
    float blink = 0.0f;
    if (m_blinkTimer >= m_blinkOpenEnd && m_blinkTimer <= m_blinkClosedEnd) {
        blink = 1.0f;
    } else if (m_blinkTimer > m_blinkClosedEnd && m_blinkTimer < m_blinkCloseEnd) {
        blink = 1.0f - (m_blinkTimer - m_blinkClosedEnd) / (m_blinkCloseEnd - m_blinkClosedEnd);
    } else if (m_blinkTimer < m_blinkOpenEnd) {
        blink = m_blinkTimer / m_blinkOpenEnd;
    }
    m_face.SetBlendTargetWeight(0xD, blink);
    if (m_jaw.m_prevMode >= 0) {
        m_face.SetBlendTargetWeight(m_jaw.m_prevMode, 0.0f);
    }
    m_jaw.Update((int)dt);
    m_face.SetBlendTargetWeight(m_jaw.m_mode, m_jaw.m_open);
}

// Not in its own player's view.
void CAnimatedPlayer::Draw(CDrawContext& ctx) {
    if (ctx.field_0x114 != (int)m_pPlayer->field_0x768) {
        CSoldierObject::Draw(ctx);
    }
}

void CAnimatedPlayer::HandleBulletCollision(CBullet* bullet, const CCollision& clsn) {
    if (m_pPlayer == bullet->GetFiredBy()) {
        // The player's own shot: let it through.
        g_scene.ExcludeCollision(*this, *bullet, 5);
        bullet->Penetrate(clsn);
        return;
    }
    if (bullet->AsProjectile() != NULL) {
        const SClsnContact* contact = clsn.m_pContacts;
        CParticleSystem* ps = CParticleSystem::Create(0x6DA45FE8);
        if (ps != NULL) {
            // The puff points out of the surface.
            CMatrix m;
            m.Ident();
            CVector3 up(contact->m_normal);
            CVector3 front;
            CVector3 side;
            CVector3 x(1.0f, 0.0f, 0.0f);
            if ((float)fabs(up.z * x.z + (up.x * x.x + up.y * x.y)) <= 0.9999f) {
                front.Cross(up, x);
                front.Normalize();
                side.Cross(front, up);
                side.Normalize();
            } else {
                CVector3 z(0.0f, 0.0f, 1.0f);
                side.Cross(up, z);
                side.Normalize();
                front.Cross(up, side);
                front.Normalize();
            }
            m.SetRight(side);
            m.SetFront(front);
            m.SetUp(contact->m_normal);
            m.SetPos(contact->m_point);
            ps->SetLocalToWorld(m);
        }
    }
    m_pPlayer->HandleBulletCollision(bullet, clsn);
}

void CAnimatedPlayer::InitAI() {}

// CSoldierObject::Init without the head matrix, hit spheres and machine
// gun weapon; collision id 0x1C, a bigger centred hit volume, and the
// multiplayer mood.
int CAnimatedPlayer::Init() {
    CAnimObject::Init();
    memset(m_attached, 0, sizeof(m_attached));
    memset(m_attachType, 0, sizeof(m_attachType));
    memset(m_pWeapons, 0, sizeof(m_pWeapons));
    m_pFloatingObject = NULL;
    m_face.Init(this, 0, &m_anim.m_frame);
    m_moodTarget = -1;
    m_nextExpression = -1;
    m_expression = -1;
    m_nextExpressionWeight = 0.0f;
    m_expressionWeight = 0.0f;
    m_nextExpressionTarget = 0.0f;
    m_expressionTarget = 0.0f;
    m_nextExpressionRate = 0.0f;
    m_expressionRate = 0.0f;
    int r = rand() % 100;
    if (r < 25) {
        m_defaultExpression = 13;
    } else if (r < 50) {
        m_defaultExpression = 6;
    } else if (r < 75) {
        m_defaultExpression = 7;
    } else {
        m_defaultExpression = 5;
    }
    if (g_bInMultiplayerMode) {
        SetMood((EMoodType)2);
    } else {
        SetMood((EMoodType)0);
    }
    m_faceStateQueued = 0;

    CAnimatedVolume* hit = AllocateAnimatedVolume();
    hit->SetRadius(1.2f);
    hit->SetCenter(CVector3(0.0f, 0.0f, 0.0f));
    hit->m_pOwner = this;
    m_pHitVolume = hit;
    m_pLocalVolume = hit;
    m_pWorldVolume = AllocateAnimatedVolume();
    CVolBox* box = AllocateBoxVolume();
    box->SetBasis(CVector3(1.0f, 0.0f, 0.0f), CVector3(0.0f, 1.0f, 0.0f), CVector3(0.0f, 0.0f, 1.0f));
    box->SetWidth(0.75f);
    box->SetHeight(1.6f);
    box->SetDepth(0.6f);
    box->SetCenter(CVector3(0.0f, 0.0f, 0.8f));
    m_pLocalBound = box;
    m_pWorldBound = AllocateBoxVolume();
    SetCollisionId((EClsnId)0x1C);
    m_curWeapon = 0;

    // (The original repeats this block three times.)
    m_headTrans = 0;
    m_headTransOffset.z = 0.0f;
    m_headTransOffset.y = 0.0f;
    m_headTransOffset.x = 0.0f;
    m_floatingBoneOn = 0;
    m_hasEmitter = 0;
    m_soundHandle = -1;
    field_0x43A4 = 1;
    m_helmetChance = 40;
    m_helmetHitScale = 0.75f;
    m_fading = 0;
    field_0x43BC = 0.0f;
    m_shadow.m_pOwner = this;
    m_shadow.m_alphaScale = 1.0f;
    m_headTrackPlayer = 0;
    m_headTrackSteps = 0;
    m_headTrackStep = 0;
    m_soundInfo.field_0x1C = 1;
    m_soundInfo.field_0x10 = 0.0f;
    *(f32*)&m_soundInfo.field_0x0 = 0.0f;
    *(f32*)&m_soundInfo.field_0x4 = 0.0f;
    *(f32*)&m_soundInfo.field_0x8 = 0.0f;
    m_soundInfo.field_0xC = 0;
    m_soundInfo.field_0x14 = 0;
    m_noNonVisDestruct = 0;
    field_0x3161 = 0;
    m_mmgFired = 0;
    m_pMMG = NULL;
    m_pMMGWeapon = NULL;
    m_burstTimer = 0.0f;
    m_burstHits = 0;
    m_headTrans = 0;
    m_headTransOffset.z = 0.0f;
    m_headTransOffset.y = 0.0f;
    m_headTransOffset.x = 0.0f;
    m_flocoTime = 0.0f;
    m_flocoState = 0;
}

CAnimatedPlayer::~CAnimatedPlayer() {}

CAnimatedPlayer::CAnimatedPlayer() {
    m_pPlayer = NULL;
}
