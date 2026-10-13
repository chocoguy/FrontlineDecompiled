// CPlayerObject: the player (see player.h).
//
#include "game/player.h"
#include "game/player_weapon_object.h"
#include "game/animplayer.h"
#include "game/Soldier_object.h"
#include "game/weapon.h"
#include "game/weaponfactory.h"
#include "game/hierobject.h"
#include "game/static_obj.h"
#include "game/AIFilter.h"
#include "game/AIObject.h"
#include "game/AnimObjFactory.h"
#include "game/mainloop.h"
#include "game/scene.h"
#include "game/collision.h"
#include "game/SClsnContact.h"
#include "game/line.h"
#include "game/cdbobject.h"
#include "game/bullet.h"
#include "game/projectilebullet.h"
#include "game/thrownbullet.h"
#include "game/draw_context.h"
#include "game/sound.h"
#include "game/input.h"
#include "game/fader.h"
#include "game/CFadeOut.h"
#include "game/color.h"
#include "game/Pause.h"
#include "game/ShellMenu.h"
#include "game/bsbifunc.h"
#include "game/pumhandler.h"
#include "game/sprite.h"
#include "game/surfacetype.h"
#include "game/fiber.h"
#include "game/particlesystem.h"
#include "game/thrown_obj.h"
#include "game/AISplinePath.h"
#include "game/propdat.h"
#include "game/toilet.h"
#include "game/memory.h"
#include "game/Screen.h"
#include "game/staticmesh.h"
#include "game/viewport.h"
#include "game/proxtrig.h"
#include "game/lightvolman.h"
#include "game/bsevents.h"
#include "game/bsobject.h"
#include "game/bstypes.h"
#include "game/BSGO_Basic.h"
#include "game/objcreate.h"
#include "game/MathFun.h"
#include "game/Moh2.h"
#include "game/gameconfig.h"
#include "math.h"
#include "stdio.h"
#include "string.h"

float DEFAULT_FOV = 35.0f;
// Explosions started by scripts push the player by these (level settings).
float g_fScriptExplosionVelocityScaleOnPlayer;
float g_fScriptExplosionHeightScaleOnPlayer;

// The camera's offset from the feet: standing, crouched (single player and
// multiplayer); g_CameraOffset is the current one.
static CVector3 CAMERA_STAND_OFFSET(0.0f, 0.0f, 1.2f);
static CVector3 CAMERA_CROUCH_OFFSET_SP(0.0f, 0.0f, 1.1f);
static CVector3 CAMERA_CROUCH_OFFSET_MP(0.0f, 0.0f, 0.8f);
static CVector3 g_CameraOffset(CAMERA_STAND_OFFSET);
static CScreen* g_pScreen;
static CCamera* g_pCamera;
static CSprite* g_pBazookaOverlay;
static float g_MPAnimMeshZOffset = -0.4f;  // the body is drawn this far below the feet
static float MMG_TURNSPEED = 0.015f;
static int RESPAWN_FIRE_DELAY = 0x78;  // multiplayer, frames

// The eye height standing and crouched.
#define EYE_STAND 1.1f
#define EYE_CROUCH_SP 0.0f
#define EYE_CROUCH_MP 0.7f

// (A weak copy in the original's player object; other units use it too.)
CLine3::CLine3(CVector3 start, CVector3 end) {
    m_start = start;
    m_end = end;
    m_dir.x = m_end.x - m_start.x;
    m_dir.y = m_end.y - m_start.y;
    m_dir.z = m_end.z - m_start.z;
    m_lengthValid = 0;
    m_lengthSqValid = 0;
}

// AEMS player sound event: `sub` on the current surface sound.
static inline void PlayerSound(int sub, int surface) {
    int event[7];
    event[0] = 4;
    event[1] = sub;
    event[2] = surface;
    event[3] = 1;
    event[4] = 0;
    event[5] = 0;
    event[6] = 0;
    AEMS_SendEvent(event);
}

// The shot statistics' body region of a hit location.
static inline EShotLocations ShotLocation(int location) {
    switch (location) {
    case 3:
    case 4:
    case 5:
        return (EShotLocations)1;
    case 6:
    case 7:
    case 8:
        return (EShotLocations)2;
    case 13:
    case 14:
    case 15:
        return (EShotLocations)4;
    case 16:
    case 17:
    case 18:
        return (EShotLocations)5;
    case 19:
    case 20:
        return (EShotLocations)6;
    case 0:
    case 1:
        return (EShotLocations)0;
    case 2:
    case 9:
    case 10:
    case 11:
    case 12:
    default:
        return (EShotLocations)3;
    }
}

// Multiplayer: drops the current weapon (with its ammo) where the player
// died.
void CPlayerObject::DropMPDumpObject() {
    CPlayerWeaponObject* current = m_weapons[m_curWeapon];
    if (current != NULL) {
        CWeapon* weapon = current->m_pWeapon;
        CQuaternion rot;  // identity
        rot.w = 1.0f;
        rot.x = 0.0f;
        rot.y = 0.0f;
        rot.z = 0.0f;
        CVector3 pos(*(CVector3*)m_tm.m[3]);
        CreateDumpObject(pos, rot, *(int*)weapon->m_pProperties, weapon->m_reserveAmmo + weapon->m_clipAmmo);
    }
}

// Multiplayer: the shooter's statistics for a hit on this player.
void CPlayerObject::SetMPShotLocation(BS_STRUCT_HitByBulletInfo_struct* hit) {
    CPlayerObject* shooter = hit->m_firedBy->m_gameObject->GetSceneNode()->AsPlayerObject();
    if (hit->m_damage != 0 && *(int*)m_pScriptObject->m_gameObject->GetScriptData() > 0) {
        g_Shell.Add_MPShotLocation(shooter->m_playerIndex, m_playerIndex, 1, ShotLocation((int)hit->m_pPoly));
    }
}

const char* CPlayerObject::GetPlayerName() {
    return m_name;
}

void CPlayerObject::SetHideWeapon(bool hide) {
    u32 hidden = m_hideWeapon;
    if (hidden && !hide) {
        TriggerScriptEvent(0x113, NULL, false);
    } else if (!hidden && hide) {
        TriggerScriptEvent(0x112, NULL, false);
    }
    m_hideWeapon = hide;
}

bool CPlayerObject::IsPlayerCrouching() const {
    if (g_bInMultiplayerMode) {
        bool crouching = false;
        if (m_onMG || m_eyeHeight - EYE_CROUCH_MP < 0.24000002f) {
            crouching = true;
        }
        return crouching;
    }
    bool crouching = false;
    if (m_onMG || m_eyeHeight < 0.66f) {
        crouching = true;
    }
    return crouching;
}

int CPlayerObject::IsPlayerJumping() const {
    return m_jumping;
}

// Footsteps (faster with speed), jumping and landing sounds, and the noise
// the AI hears. `fall` is the landing speed, `dt` in frames.
void CPlayerObject::PlaySounds(bool step, bool jump, float fall, float dt) {
    if (m_surfaceSound == 0) {
        m_surfaceSound = 1;
    }
    if (jump) {
        PlayerSound(0, m_surfaceSound);
    }
    if (fall > 0.25f) {
        if (fall > 4.0f) {
            // A hard landing: the player grunts.
            m_landingTimer = 10;
            int event[7];
            event[0] = 4;
            event[2] = m_surfaceSound;
            event[3] = 1;
            event[4] = 0;
            event[5] = 0;
            event[6] = 0;
            event[1] = 0x12;
            AEMS_SendEvent(event);
            AEMS_SendEvent(event);
            event[1] = 9;
            AEMS_SendEvent(event);
            event[1] = 0x14;
            AEMS_SendEvent(event);
        } else {
            m_landingTimer = 1;
        }
        MakeNoise(this, 0x33, 0.5f, 1);
    }
    if (m_landingTimer != 0) {
        m_landingTimer -= (int)(0.5f + dt);
        if (m_landingTimer <= 0) {
            int event[7];
            event[0] = 4;
            event[2] = m_surfaceSound;
            event[3] = 1;
            event[4] = 0;
            event[5] = 0;
            event[6] = 0;
            event[1] = 9;
            AEMS_SendEvent(event);
            event[1] = 0x14;
            AEMS_SendEvent(event);
            m_landingTimer = 0;
        }
    }
    float speedSq = m_velocity.z * m_velocity.z + (m_velocity.x * m_velocity.x + m_velocity.y * m_velocity.y);
    if (m_onPath || speedSq < 0.00001f) {
        // Standing still: a last step if one is due.
        if (m_stepTimer > 0.0f && (step || m_climbing)) {
            PlayerSound(4, m_climbing ? 6 : m_surfaceSound);
            MakeNoise(this, 0x33, 0.0f, 1);
        }
        m_stepReady = 1;
        m_stepTimer = 0.0f;
        return;
    }
    float speed = sqrtf(speedSq);
    float run = speed / 0.08074742f;
    if (run > 1.0f) {
        run = 1.0f;
    } else if (run < 0.0f) {
        run = 0.0f;
    }
    if (m_stepTimer > 0.0f) {
        m_stepTimer = -((0.016683351f * dt) - m_stepTimer);
    }
    if (m_stepTimer <= 0.0f) {
        if (m_stepReady || m_climbing) {
            m_stepTimer = 0.37f;
        } else {
            m_stepTimer = 1.32f - run;
        }
        if (step || m_climbing) {
            int event[7];
            event[0] = 4;
            event[1] = (int)((4.0f * run) + 4.5f);
            event[2] = m_climbing ? 6 : m_surfaceSound;
            event[3] = 1;
            event[4] = 0;
            event[5] = 0;
            event[6] = 0;
            AEMS_SendEvent(event);
            m_stepReady = 0;
            MakeNoise(this, 0x33, (2.0f * speedSq) / 0.00841961f, 1);
        }
    }
}

// Moves the eyes toward crouched (amount 1) from standing.
void CPlayerObject::DuckDown(float amount) {
    if (m_canCrouch) {
        float crouch = g_bInMultiplayerMode ? EYE_CROUCH_MP : EYE_CROUCH_SP;
        m_eyeHeightRate = (0.04170838f * (((crouch * amount) + (EYE_STAND * (1.0f - amount))) - m_eyeHeight)) / 0.1f;
    }
}

// Moves the eyes toward standing (amount 1) from crouched.
void CPlayerObject::PeekUp(float amount) {
    float crouch = g_bInMultiplayerMode ? EYE_CROUCH_MP : EYE_CROUCH_SP;
    m_eyeHeightRate = (0.04170838f * (((EYE_STAND * amount) + (crouch * (1.0f - amount))) - m_eyeHeight)) / 0.1f;
}

// Leaning: the peek input moves the player sideways along its right axis
// from where the peek started.
void CPlayerObject::PeekLeftRight() {
    float input = (float)g_inputMgr.GetValue(m_controllerPort, 8);
    m_peekStarted = 0;
    float peek = 0.0078125f * input;
    if ((float)fabs(peek) > 0.01f) {
        if (!m_peeking) {
            m_peekStarted = 1;
        }
        m_peeking = 1;
    } else {
        m_peeking = 0;
    }
    m_peekAxis = *(CVector3*)m_tm.m[0];
    m_peekYaw = m_yaw;
    CVector3 pos(*(CVector3*)m_tm.m[3]);
    CVector3 move;
    move.x = ((peek * m_peekAxis.x) + m_peekOrigin.x) - pos.x;
    move.y = ((peek * m_peekAxis.y) + m_peekOrigin.y) - pos.y;
    move.z = ((peek * m_peekAxis.z) + m_peekOrigin.z) - pos.z;
    move.z = 0.0f;
    float len = sqrtf(move.x * move.x + move.y * move.y + 0.0f * 0.0f);
    if (len > 0.1f) {
        float k = 0.04170838f / len;
        move.x *= k;
        move.y *= k;
        move.z *= k;
    } else {
        move.x *= 0.04170838f;
        move.y *= 0.04170838f;
        move.z *= 0.04170838f;
    }
    CMatrix inv;
    inv.FastInverse(m_tm);
    CVector3 local;
    inv.TransformVector(local, move);
    m_localVelocity.x = local.x;
    m_localVelocity.y = local.y;
}

void CPlayerObject::BeginCrouch() {
    if (m_canCrouch) {
        m_crouched = 1;
        m_crouchMoving = 1;
        m_eyeHeightRate = -(g_bInMultiplayerMode ? 0.02666667f : 0.07333334f);
    }
}

// Stands up unless a standing capsule would hit something.
void CPlayerObject::BeginStand() {
    CLine3 saved(m_localCapsule.m_line);
    float radius = m_localCapsule.m_radius;
    float rate = g_bInMultiplayerMode ? 0.02666667f : 0.07333334f;
    static CLine3 standingAxis(CVector3(0.0f, 0.0f, 0.0f), CVector3(0.0f, 0.0f, EYE_STAND));
    if (&standingAxis != &m_localCapsule.m_line) {
        m_localCapsule.m_line = standingAxis;
    }
    m_localCapsule.m_radius = radius;
    m_worldCapsule.TransformedCopy(m_localCapsule, m_tm);
    CCollision clsn;
    CCDBObject::DisablePassThru(true);
    int result = g_scene.m_world->GetWorldBoundingVolume((ISceneNode::EVolumeType)0)->TestCollision(m_worldCapsule, clsn, false);
    CCDBObject::DisablePassThru(false);
    if (&saved != &m_localCapsule.m_line) {
        m_localCapsule.m_line = saved;
    }
    m_localCapsule.m_radius = radius;
    m_worldCapsule.TransformedCopy(m_localCapsule, m_tm);
    if (result != CLSN_PENETRATING) {
        m_crouched = 0;
        m_crouchMoving = 1;
        m_eyeHeightRate = rate;
    }
}

// Records the level as finished: health and, per weapon, ammo (only the
// weapon 8 in mission 2 stage 2).
void CPlayerObject::SetFinishedLevelData() {
    if (!g_Shell.field_0xF) {
        return;
    }
    int mission;
    int stage;
    if (g_bInMultiplayerMode) {
        mission = g_Shell.m_mpMission;
        stage = g_Shell.m_mpStage;
    } else {
        mission = g_Shell.Get_currentMission();
        stage = g_Shell.Get_currentStage();
    }
    g_Shell.SetPlayersFinishedLevelSuccessfully(mission, stage);
    g_Shell.SetPlayersFinishedLevelHealth(mission, stage,
                                          (float)*(int*)m_pScriptObject->m_gameObject->GetScriptData());
    for (int i = 0; i < NUM_WEAPONS; i++) {
        CPlayerWeaponObject* slot = m_weapons[i];
        if (slot == NULL) {
            continue;
        }
        CWeapon* weapon = slot->m_pWeapon;
        if (weapon == NULL) {
            continue;
        }
        s16 reserve = weapon->m_reserveAmmo;
        s16 clip = weapon->m_clipAmmo;
        if ((u32)g_Shell.Get_currentMission() == 2 && (u32)g_Shell.Get_currentStage() == 2 &&
            !g_bInMultiplayerMode && i != 8) {
            continue;
        }
        u32 ammo = reserve + clip;
        g_Shell.SetPlayersFinishedLevelAmmo(mission, stage, (EWeaponTypes)i, ammo);
        if (i == 8) {
            g_Shell.SetPlayersFinishedLevelAmmo(mission, stage, (EWeaponTypes)7, ammo);
        }
    }
}

void CPlayerObject::TransitionFOV(float fov, float rate) {
    if (m_fov != fov) {
        m_fovTarget = fov;
        if (m_fovTarget < m_fov) {
            m_fovRate = -rate;
            return;
        }
        m_fovRate = rate;
    }
}

// The camera sways down as the player dies (multiplayer: turns to the body's
// view); at the end raises script event 0x78.
void CPlayerObject::UpdateDeathSequence(float dt) {
    float left = m_deathTime / 180.0f;
    float t = 1.0f - left;
    float yawRate = 0.05f * (float)cos(3.1415927f * (2.5f * t));
    float yaw = dt * yawRate;
    float pitchRate = (0.035f * (float)cos(3.1415927f / (0.5f + t))) / ((2.0f * t) + 1.0f);
    m_deathTime -= dt;
    float pitch = dt * pitchRate;
    if (!g_bInMultiplayerMode) {
        m_yaw += yaw;
        m_pitch -= pitch;
    } else {
        CMatrix look;
        m_pBody->GetLookMatrix(look);
        CVector3 fwd(*(CVector3*)look.m[1]);
        float target = (float)atan2(fwd.x, fwd.y);
        m_yaw = t * (target - m_deathYaw) + m_deathYaw;
        m_pitch = t * ((-3.1415927f * fwd.z) - m_deathPitch) + m_deathPitch;
    }
    if (m_deathTime <= 0.0f) {
        m_deathTime = 0.0f;
        TriggerScriptEvent(0x78, NULL, false);
        return;
    }
    if (!g_bInMultiplayerMode) {
        SoundSetMasterVolume((int)(127.0f * left));
    }
}

// Drops the weapon, crouches and (single player) fades the screen out.
void CPlayerObject::StartDeathSequence() {
    if (m_deathTime != 0.0f) {
        return;
    }
    m_dead = 1;
    m_collisionId = 0x19;
    if (m_curWeapon >= 0) {
        m_weapons[m_curWeapon]->m_active = 0;
        m_weapons[m_curWeapon]->Shoot((EWeaponShootType)1, 0);
        CWeapon* weapon = m_weapons[m_curWeapon]->m_pWeapon;
        if (weapon != NULL) {
            weapon->m_refireTime = 0.0f;
        }
    }
    m_deathTime = 180.0f;
    m_attachedKeepWeapon = 0;
    BeginCrouch();
    if (!g_bInMultiplayerMode) {
        static CFadeOut fadeOut(CColor(0, 0, 0, 0), CColor(0, 0, 0, 0x80), 180.0f);
        CFader::SetControl(&fadeOut);
        g_Pause.SetState((PauseState)7);
    }
    m_deathYaw = m_yaw;
    m_deathPitch = m_pitch;
}

void CPlayerObject::ExitLightVolume(BPDLightVolume* volume) {
    for (int i = 0; i < NUM_WEAPONS; i++) {
        if (m_weapons[i] != NULL) {
            m_weapons[i]->ExitLightVolume(volume);
        }
    }
}

void CPlayerObject::EnterLightVolume(BPDLightVolume* volume) {
    for (int i = 0; i < NUM_WEAPONS; i++) {
        if (m_weapons[i] != NULL) {
            m_weapons[i]->EnterLightVolume(volume);
        }
    }
}

// The weapon in hand (the mounted gun while on one).
static inline CWeapon* HeldWeapon(const CPlayerObject* player) {
    if (player->m_onMG) {
        return player->m_pMGWeapon;
    }
    if (player->m_curWeapon >= 0) {
        return player->m_weapons[player->m_curWeapon]->m_pWeapon;
    }
    return NULL;
}

void CPlayerObject::StartGrenadeCook() {
    if (HeldWeapon(this)->CanCook()) {
        m_cookTime = 240.0f;
    }
}

// Fires the weapon in hand with `power` (grenades: thrown harder the longer
// the button was held) and kicks the view.
void CPlayerObject::FireMyWeapon(float power) {
    CPlayerWeaponObject* current;
    if (m_onMG) {
        current = NULL;
    } else if (m_curWeapon >= 0) {
        current = m_weapons[m_curWeapon];
    } else {
        current = NULL;
    }
    if (current != NULL) {
        CPlayerWeaponObject* recoil;
        if (m_onMG) {
            recoil = NULL;
        } else if (m_curWeapon >= 0) {
            recoil = m_weapons[m_curWeapon];
        } else {
            recoil = NULL;
        }
        m_recoilPitch += recoil->field_0x3190;
        m_recoilYaw += recoil->field_0x318C;
    }
    int type = HeldWeapon(this)->m_weaponType;
    if (type == 0x21 || type == 0x22) {
        if (power < 0.0f) {
            power = 0.0f;
        }
        power = 220.0f * (float)sin(1.5707964f * (power / 255.0f));
    }
    HeldWeapon(this)->Shoot((EWeaponShootType)0, power, m_cookTime);
    m_cookTime = -1.0f;
}

void CPlayerObject::SetFallingDamageState(bool on) {
    m_fallingDamage = on;
}

void CPlayerObject::SetClimbingState(bool on) {
    m_climbing = on;
    if (m_climbing) {
        m_falling = 0;
    }
}

void CPlayerObject::TriggerScriptEvent(int event, void* data, bool immediate) {
    if (m_pScriptObject != NULL) {
        BSObjectTriggerEvent(m_pScriptObject, event, data, NULL, immediate);
    }
}

void CPlayerObject::SetScript(BSObject* script) {
    m_pScriptObject = script;
    for (int i = 0; i < NUM_WEAPONS; i++) {
        if (m_weapons[i] != NULL) {
            m_weapons[i]->SetScript(script);
        }
    }
}

void CPlayerObject::StopPath() {
    m_onPath = 0;
}

// Moves the player along `path` (looking along `look`) over `time` seconds.
void CPlayerObject::MoveOnPath(CAISplinePath* path, CAISplinePath* look, float time) {
    m_onPath = 1;
    m_pPath = path;
    m_pLookPath = look;
    m_pathPos = 0.0f;
    m_pathRate = 0.016683351f * (1.0f / time);
}

// Adds the camera shake and the motion shake (stronger the faster the
// player moves) to the view angles.
void CPlayerObject::ShakeCamera(float& yaw, float& pitch, float dt) {
    if (m_cameraShake) {
        float time = m_shakeTime;
        float scale = -((time * m_shakeFadeRate) - 1.0f);
        if (time > 0.0f) {
            m_shakeTime = time - dt;
            if (m_shakeTime <= 0.0f) {
                m_shakeTime = 0.0f;
            }
        } else if (time < 0.0f) {
            scale = 1.0f - scale;
            m_shakeTime = time + dt;
            if (m_shakeTime >= 0.0f) {
                m_shakeTime = 0.0f;
                m_cameraShake = 0;
            }
        }
        float amount = 0.25f * m_shakeAmount * scale;
        pitch += MathFunRandomReal(-amount, amount);
        yaw += MathFunRandomReal(-amount, amount);
    }
    if (m_motionShakeOn) {
        float step = m_motionShakeMax / m_motionShakeTime;
        float speed = (m_velocity.z * m_velocity.z + (m_velocity.x * m_velocity.x + m_velocity.y * m_velocity.y)) /
                      0.00841961f;
        if (speed > 1.0f) {
            speed = 1.0f;
        }
        if (speed * m_motionShakeMax >= m_motionShake) {
            m_motionShake = m_motionShake + step;
        } else {
            m_motionShake = m_motionShake - step;
        }
        if (m_motionShake > m_motionShakeMax) {
            m_motionShake = m_motionShakeMax;
        } else if (m_motionShake < 0.0f) {
            m_motionShake = 0.0f;
        }
        float amount = 0.25f * m_motionShake;
        pitch += MathFunRandomReal(-amount, amount);
        yaw += MathFunRandomReal(-amount, amount);
    }
}

void CPlayerObject::DoMotionShake(float amount, float time) {
    m_motionShakeMax = amount;
    m_motionShakeTime = time;
    m_motionShake = 0.0f;
    if (amount > 0.0f) {
        m_motionShakeOn = 1;
        return;
    }
    m_motionShakeOn = 0;
}

void CPlayerObject::StopCameraShake(float time) {
    if (time < 0.0f) {
        time = -time;
    }
    m_shakeTime = -time;
    if (time == 0.0f) {
        time = 1.0f;
        m_cameraShake = 0;
    }
    m_shakeFadeRate = -1.0f / time;
}

void CPlayerObject::StartCameraShake(float amount, float time) {
    if (time < 0.0f) {
        time = -time;
    }
    m_shakeAmount = amount;
    m_shakeTime = time;
    if (time == 0.0f) {
        time = 1.0f;
    }
    m_shakeFadeRate = 1.0f / time;
    m_cameraShake = 1;
}

int CPlayerObject::GetCurrentWeaponReserveAmoCount() const {
    if (m_onMG) {
        return m_pMGWeapon->m_reserveAmmo;
    }
    if (m_curWeapon >= 0) {
        return m_weapons[m_curWeapon]->m_pWeapon->m_reserveAmmo;
    }
    return 0;
}

int CPlayerObject::GetCurrentWeaponClipAmoCount() const {
    if (m_onMG) {
        return m_pMGWeapon->m_clipAmmo;
    }
    if (m_curWeapon >= 0) {
        return m_weapons[m_curWeapon]->m_pWeapon->m_clipAmmo;
    }
    return 0;
}

CSprite* CPlayerObject::GetCurrentBulletSprite() const {
    if (m_onMG) {
        return m_pMGWeapon->GetBulletSprite();
    }
    if (m_curWeapon >= 0) {
        return m_weapons[m_curWeapon]->m_pWeapon->GetBulletSprite();
    }
    return NULL;
}

CPlayerWeaponObject* CPlayerObject::GetWeaponByCRC(int crc) const {
    return m_weapons[GetWeaponInfoByCRC(crc, NULL, NULL)];
}

// A weapon's type from its CRC (any of its names), and the HUD strings
// for it (-1 none).
int CPlayerObject::GetWeaponInfoByCRC(int crc, int* name, int* pickup) {
    int dummy;
    if (name == NULL) {
        name = &dummy;
    }
    if (pickup == NULL) {
        pickup = &dummy;
    }
    switch ((u32)crc) {
    case 0x5C95B07A:
    case 0xC320DF3B:
    case 0x8EFB8E12:
        *name = 0x15C;
        *pickup = 0x216;
        return 0xE;
    case 0x392805E6:
    case 0xCC7202DA:
    case 0xD8541A59:
        *name = 0x15D;
        *pickup = 0x217;
        return 0x14;
    case 0xF45E50D5:
    case 0xCC0DC130:
    case 0xAF3A77E7:
        *name = 0x15E;
        *pickup = 0x10E;
        return 0x1D;
    case 0xBEAA9141:
    case 0x9884DA57:
        *name = 0x15F;
        *pickup = 0x215;
        return 7;
    case 0x4804D932:
    case 0x36EAD81A:
    case 0xADC42548:
        *name = 0x160;
        *pickup = 0x21A;
        return 0x10;
    case 0x6E584C98:
    case 0xF6CFC618:
    case 0x985B620E:
        *name = 0x161;
        *pickup = 0x21B;
        return 0xF;
    case 0x0C839118:
    case 0xC662E91E:
    case 0x8E32A26E:
        *name = 0x162;
        *pickup = 0x218;
        return 0x15;
    case 0x4405E37B:
    case 0x9D0E7C6C:
        *name = 0x163;
        *pickup = 0x21C;
        return 0x19;
    case 0x6B16E90B:
    case 0x2446755F:
        *name = 0x164;
        *pickup = 0x21D;
        return 0x18;
    case 0xA2401755:
    case 0x900B140B:
        *name = 0x16A;
        *pickup = 0x110;
        return 0x1E;
    case 0x1E042F0C:
    case 0xA55DDA2D:
        *name = -1;
        *pickup = 0x10F;
        return 0x21;
    case 0xFAD7222B:
    case 0xA8B9679D:
        *name = 0x165;
        *pickup = 0x21E;
        return 5;
    case 0x36ADF7F0:
    case 0x87A43CCA:
    case 0x87038E92:
        *name = 0x167;
        *pickup = 0x21F;
        return 8;
    case 0xEF098CF1:
    case 0x47285958:
        *name = 0x168;
        *pickup = -1;
        return 0x24;
    case 0x2FA05716:
    case 0x29FF7E9E:
        *name = 0x169;
        *pickup = -1;
        return 0x23;
    case 0x13DB5201:
    case 0xD1F65C59:
        *name = -1;
        *pickup = -1;
        return 0x25;
    case 0x4F79C15B:
    case 0x112A938A:
        *name = 0x16B;
        *pickup = 0x220;
        return 0x20;
    case 0x585FF990:
    case 0x1A496FAB:
    case 0x83674487:
        *name = -1;
        *pickup = 0x221;
        return 0x22;
    case 0x72787373:
    case 0xC186CA3C:
    case 0x88AC7990:
        *name = 0x166;
        *pickup = 0x219;
        return 6;
    default:
        DebugMsg("Unrecognized player weapon CRC: %d\n", crc);
        *name = -1;
        *pickup = -1;
        return 0xE;
    }
}

CPlayerWeaponObject* CPlayerObject::GetWeapon(int type) const {
    return m_weapons[type];
}

CWeapon* CPlayerObject::GetCurrentWeapon() const {
    return HeldWeapon(this);
}

CPlayerWeaponObject* CPlayerObject::GetCurrentPlayerWeapon() const {
    if (m_onMG) {
        return NULL;
    }
    if (m_curWeapon >= 0) {
        return m_weapons[m_curWeapon];
    }
    return NULL;
}

// Switches to the next (dir > 0) or previous selectable weapon, or to
// weapon `only` if it is selectable. Multiplayer: the body takes the
// weapon in hand too.
int CPlayerObject::CycleWeapon(int dir, int only) {
    for (int step = 1; step < NUM_WEAPONS; step++) {
        int next;
        if (dir > 0) {
            next = m_curWeapon + step;
        } else {
            next = m_curWeapon - step;
        }
        if (next >= NUM_WEAPONS) {
            next -= NUM_WEAPONS;
        } else if (next < 0) {
            next += NUM_WEAPONS;
        }
        if (only >= 0 && next != only) {
            continue;
        }
        if (m_weapons[next] == NULL || !m_weapons[next]->IsSelectable()) {
            continue;
        }
        if (m_curWeapon >= 0 && m_weapons[m_curWeapon] != NULL) {
            m_weapons[m_curWeapon]->Shoot((EWeaponShootType)1, 0);
            m_weapons[m_curWeapon]->m_active = 0;
            m_weapons[m_curWeapon]->m_pWeapon->m_bReloading = 0;
        }
        m_curWeapon = next;
        m_weapons[m_curWeapon]->m_active = 1;
        int* data = (int*)m_pScriptObject->m_gameObject->GetScriptData();
        data[2] = (int)m_weapons[m_curWeapon];
        if (m_weapons[m_curWeapon] != NULL) {
            data[1] = m_weapons[m_curWeapon]->m_pWeapon->m_weaponType;
        } else {
            data[1] = -1;
        }
        if (g_bInMultiplayerMode) {
            CAnimatedPlayer* body = m_pBody;
            CWeapon* old = body->m_pWeapons[body->m_curWeapon];
            int slot = m_bodyWeaponSlot[m_curWeapon];
            if (old != NULL) {
                old->Update(1.0f, false);
                body->m_curWeapon = slot;
                char* hand;
                int handPoint;
                if (!(body->m_pWeapons[body->m_curWeapon]->m_pProperties->m_hand & 1)) {
                    hand = "lt_hand";
                    handPoint = 0x4C83C769;
                } else {
                    hand = "rt_hand";
                    handPoint = 0xD58A96D3;
                }
                if (g_bInMultiplayerMode) {
                    body->DetachObject("lt_hand");
                    body->DetachObject("rt_hand");
                } else {
                    body->DetachObject(hand);
                }
                body->AttachObject(body->m_pWeapons[body->m_curWeapon], hand, handPoint);
                if (!g_bInMultiplayerMode) {
                    ((CAIFilterSoldierObject*)body->m_aiDoodad.m_pFilter)->UpdateManualUpdateVars();
                }
                body->SetBulletEmitter(body->m_pWeapons[body->m_curWeapon], hand);
            } else {
                body->m_curWeapon = slot;
            }
        }
        int event[7];
        event[0] = 0xA;
        event[1] = (s16)m_weapons[m_curWeapon]->m_pWeapon->GetWeaponSoundType();
        event[2] = 0;
        event[3] = 0;
        event[4] = 0;
        event[5] = 0;
        event[6] = 0;
        AEMS_SendEvent(event);
        return 1;
    }
    return 0;
}

// Pushed by an explosion (along `dir`, `force` / 512 units per frame).
void CPlayerObject::ApplyForceTo(CVector3 dir, float force) {
    float k = force * 0.001953125f;
    m_pushed = 1;
    field_0x278 = dir;
    field_0x278.Normalize();
    field_0x278.x = field_0x278.x * k;
    field_0x278.y = field_0x278.y * k;
    field_0x278.z = field_0x278.z * k;
}

void CPlayerObject::GetUpward(CVector3& v) const {
    v = *(CVector3*)m_tm.m[2];
}

void CPlayerObject::GetForward(CVector3& v) const {
    v = *(CVector3*)m_tm.m[1];
}

void CPlayerObject::GetRightward(CVector3& v) const {
    v = *(CVector3*)m_tm.m[0];
}

void CPlayerObject::GetPosition(CVector3& v) const {
    v = *(CVector3*)m_tm.m[3];
}

void CPlayerObject::Orthonormalize() {
    m_tm.Orthonormalize();
}

int CPlayerObject::Yaw(float angle) {
    m_tm.RotateZ(angle);
    m_yaw += angle;
}

void CPlayerObject::Roll(float) {}

void CPlayerObject::Pitch(float angle) {
    m_pitch += angle;
}

void CPlayerObject::Rotate(CVector3 axis, float angle) {
    m_tm.Rotate(axis, angle);
}

void CPlayerObject::Move(CVector3 v) {
    m_tm.Translate(v);
}

void CPlayerObject::SetBasis(CVector3 right, CVector3 front, CVector3 up) {
    m_tm.SetRight(right);
    m_tm.SetFront(front);
    m_tm.SetUp(up);
}

void CPlayerObject::SetPosition(CVector3 pos) {
    m_tm.SetPos(pos);
}

// Takes the yaw and pitch from `tm`; the camera follows the pitch unless
// the player is carried by an object.
void CPlayerObject::SetTMLocalToWorld(const CMatrix& tm) {
    m_tm = tm;
    float roll;
    tm.ToEulerXYZ(m_pitch, roll, m_yaw);
    m_eyeTransform.BuildTrans(g_CameraOffset);
    if (!m_attached || m_attachedKeepWeapon) {
        m_eyeTransform.RotateX(m_pitch);
    }
    field_0x288 = *(CVector3*)m_tm.m[3];
}

void CPlayerObject::Transform(const CMatrix& tm) {
    m_tm.Multiply(m_tm, tm);
}

void CPlayerObject::PreTransform(const CMatrix& tm) {
    m_tm.Multiply(tm, m_tm);
}

void CPlayerObject::Halt() {
    m_localVelocity.z = 0.0f;
    m_localVelocity.y = 0.0f;
    m_localVelocity.x = 0.0f;
    m_velocity.z = 0.0f;
    m_velocity.y = 0.0f;
    m_velocity.x = 0.0f;
    m_eyeHeightRate = 0.0f;
}

void CPlayerObject::Reset() {
    m_tm.Ident();
    m_eyeTransform.Ident();
    m_localVelocity.z = 0.0f;
    m_localVelocity.y = 0.0f;
    m_localVelocity.x = 0.0f;
    m_velocity.z = 0.0f;
    m_velocity.y = 0.0f;
    m_velocity.x = 0.0f;
    field_0x288.z = 0.0f;
    field_0x288.y = 0.0f;
    field_0x288.x = 0.0f;
    m_yaw = 0.0f;
    m_pitch = 0.0f;
    m_onGround = 0;
    m_onSlope = 0;
    m_deathTime = 0.0f;
    m_groundZ = 0.0f;
    m_slopeGrip = 1.0f;
    m_eyeHeight = EYE_STAND;
    m_eyeHeightRate = 0.0f;
    field_0x740.z = 0.0f;
    field_0x740.y = 0.0f;
    field_0x740.x = 0.0f;
    for (int i = 0; i < NUM_WEAPONS; i++) {
        m_weapons[i] = NULL;
        m_bodyWeaponSlot[i] = -1;
        if (g_bInMultiplayerMode && m_pBody != NULL) {
            m_pBody->RemoveWeapons();
        }
    }
    m_pMGGun = NULL;
    m_pMGGun2 = NULL;
    m_pMGCannon = NULL;
}

int CPlayerObject::GetCollisionId() const {
    return m_collisionId;
}

int CPlayerObject::IsDrawEnabled() const {
    return 1;
}

int CPlayerObject::IsVisible(CDrawContext& ctx) const {
    if (m_pLocalBound != NULL) {
        m_pWorldBound->TransformedCopy(*m_pLocalBound, m_tm);
    }
    ctx.m_2dReject = 0x3F;
    return 1;
}

void CPlayerObject::GetTMLocalToWorld(CMatrix& tm) const {
    tm = m_tm;
}

// A health pickup (`crc`): adds `amount` up to the maximum. Returns
// whether the pickup was used (and removes it).
bool CPlayerObject::AddHealth(int crc, int amount, void* pickup) {
    bool added = false;
    int sound;
    switch ((u32)crc) {
    case 0x598E4105:
    case 0x72441AA1:
        sound = 6;
        break;
    case 0x10EFEBA2:
        sound = 7;
        break;
    case 0xF7BADDB9:
        sound = 8;
        break;
    default:
        sound = -1;
        break;
    }
    BSObject* script = GetScriptObject();
    if (script != NULL) {
        TriggerObjectData_struct* data = script->m_trigger->m_data;
        int* health = (int*)script->m_gameObject->GetScriptData();
        if (data != NULL && health != NULL) {
            int max;
            if (g_bInMultiplayerMode) {
                max = g_Shell.field_0x143C[m_controllerPort];
            } else {
                max = data->field_0x70;
            }
            int cur = *health;
            int value = cur + amount;
            if (max != cur) {
                if (sound != -1) {
                    int event[10];
                    event[0] = 7;
                    event[1] = 3;
                    event[2] = sound;
                    event[3] = MathFunRandomI64(0xED8, 0x1130);
                    event[4] = 0;
                    event[5] = 0;
                    event[6] = 0;
                    event[7] = 0x61A7;
                    event[8] = 0;
                    event[9] = 0x7D00;
                    AEMS_SendEvent(event);
                }
                added = true;
            }
            if (value > max) {
                value = max;
            }
            if (value < 0) {
                value = 0;
            }
            *health = value;
            if (value == max) {
                // Full: health pickups can't be taken.
                EnableDumpCollisionsByType((EDumpType)0, -1, false);
            }
        }
    }
    if ((added || g_bInMultiplayerMode) && pickup != NULL) {
        MarkDumpObjectForDestruction(pickup);
        return true;
    }
    return added;
}

// A weapon or ammo pickup (`crc`): a new weapon becomes selectable (and is
// taken in hand if the player has none), then `amount` ammo is added up to
// the maximum. Prompts what was picked up. Returns whether it was used.
bool CPlayerObject::AddAmmoOrWeapon(int crc, int amount, void* pickup) {
    CPlayerWeaponObject* slot = m_weapons[GetWeaponInfoByCRC(crc, NULL, NULL)];
    int sound = 9;
    CWeapon* weapon = slot->m_pWeapon;
    int name = -1;
    int ammoName = -1;
    GetWeaponInfoByCRC(crc, &name, &ammoName);
    bool ammoOnly;
    switch ((u32)crc) {
    case 0x4405E37B:
    case 0x5C95B07A:
    case 0x72787373:
    case 0x47285958:
    case 0x2FA05716:
    case 0x36EAD81A:
    case 0x2446755F:
    case 0xA8B9679D:
    case 0x0C839118:
    case 0x112A938A:
    case 0xCC7202DA:
    case 0xAF3A77E7:
    case 0x985B620E:
    case 0x9884DA57:
    case 0x900B140B:
    case 0x87A43CCA:
        ammoOnly = true;
        break;
    case 0x1A496FAB:
    case 0x1E042F0C:
        sound = 0x1D;
        ammoOnly = false;
        break;
    default:
        ammoOnly = false;
        break;
    }
    bool used = false;
    bool newWeapon = false;
    if (!slot->IsSelectable() && !ammoOnly) {
        s16 type = weapon->GetWeaponSoundType();
        if (type < 7) {
            sound = 0x1E;
        } else if (type < 0xE) {
            sound = 0x1F;
        } else if (type < 0x14) {
            sound = 0x20;
        } else if (type < 0x16) {
            sound = 0x1D;
        } else if (type < 0x18) {
            sound = 0x21;
        }
        newWeapon = true;
        slot->m_selectable = 1;
        if (name >= 0) {
            appendMessageToPrompt(GetUMString(name), field_0x768);
            displayPromptBuffer(field_0x768);
        }
        if (m_curWeapon < 0) {
            TriggerScriptEvent(0xA2, (void*)sound, false);
            CycleWeapon(1, -1);
        }
        used = true;
    }
    s16 reserve = weapon->m_reserveAmmo;
    s16 max = weapon->m_pProperties->m_maxAmmo;
    if (amount > 0 && reserve < max) {
        if (ammoName >= 0) {
            char text[0x20];
            if (max - reserve < amount) {
                sprintf(text, "%d", max - reserve);
            } else {
                sprintf(text, "%d", amount);
            }
            appendMessageToPrompt(text, field_0x768);
            appendMessageToPrompt(GetUMString(ammoName), field_0x768);
            displayPromptBuffer(field_0x768);
        }
        used = true;
        slot->AddAmo(amount);
        if (newWeapon) {
            slot->DoneReloading();
        }
        TriggerScriptEvent(0xA2, (void*)sound, false);
    }
    if ((used || g_bInMultiplayerMode) && pickup != NULL) {
        MarkDumpObjectForDestruction(pickup);
        return true;
    }
    return used;
}

void CPlayerObject::UpdateAI(float dt) {
    m_aiDoodad.Update(dt);
}

// Starts the level crouched.
void CPlayerObject::StartPlayerCrouched() {
    m_crouched = 1;
    m_crouchMoving = 0;
    m_eyeHeight = g_bInMultiplayerMode ? EYE_CROUCH_MP : EYE_CROUCH_SP;
    CLine3 axis(CVector3(0.0f, 0.0f, 0.0f), CVector3(0.0f, 0.0f, m_eyeHeight));
    if (&axis != &m_localCapsule.m_line) {
        m_localCapsule.m_line = axis;
    }
    m_worldCapsule.TransformedCopy(m_localCapsule, m_tm);
}

// On a hierarchical gun: the gun's matrix positions the player and the
// camera, and aims the gun along its muzzle hard point.
int CPlayerObject::SetPlayersMountedMGPositions(CMatrix* gunTM) {
    m_tm = *gunTM;
    m_tm.Multiply(m_mgPivotTM, m_tm);
    CMatrix cam;
    cam.Multiply(m_eyeTransform, m_tm);
    m_camera.SetTMLocalToWorld(cam);
    m_camera.AttemptUpdate(1.0f);
    CMatrix muzzle;
    muzzle = *gunTM;
    muzzle.Multiply(m_mgMuzzleTM, muzzle);
    m_pMGWeapon->m_aimDir = *(CVector3*)muzzle.m[1];
    m_pMGWeapon->m_aimOrigin = *(CVector3*)muzzle.m[3];
}

const IVolume* CPlayerObject::GetWorldBoundingVolume(ISceneNode::EVolumeType type) const {
    switch ((int)type) {
    case 3:
        return m_pWorldBound;
    case 0:
    case 1:
    case 2:
        return &m_worldCapsule;
    case 4:
    default:
        return NULL;
    }
}

const IVolume* CPlayerObject::GetLocalBoundingVolume(ISceneNode::EVolumeType type) const {
    switch ((int)type) {
    case 3:
        return m_pLocalBound;
    case 0:
    case 1:
    case 2:
        return &m_localCapsule;
    case 4:
    default:
        return NULL;
    }
}

void CPlayerObject::InitClass(CScreen* screen, CCamera* camera) {
    g_pScreen = screen;
    g_pCamera = camera;
    g_pBazookaOverlay = NULL;
}

void CPlayerObject::CleanUp() {
    m_aiDoodad.CleanUp();
}

void CPlayerObject::InitAI() {
    m_aiDoodad.Init(this);
}

// Restarts the level: the weapons' HUD sprites go, then Init.
void CPlayerObject::ReInit() {
    for (int i = 0; i < NUM_WEAPONS; i++) {
        CPlayerWeaponObject* weapon = m_weapons[i];
        if (weapon != NULL) {
            if (weapon->m_pSprite0 != NULL) {
                weapon->m_pSprite0->~CSprite();
            }
            if (m_weapons[i]->m_pSprite1 != NULL) {
                m_weapons[i]->m_pSprite1->~CSprite();
            }
            if (m_weapons[i]->m_pSprite2 != NULL) {
                m_weapons[i]->m_pSprite2->~CSprite();
            }
        }
    }
    Init();
}

bool CPlayerObject::SetWeaponEnable(EWeaponTypes type, bool on) {
    CPlayerWeaponObject* weapon = m_weapons[type];
    if (weapon != NULL) {
        weapon->m_selectable = on;
        return true;
    }
    return false;
}

bool CPlayerObject::CanPlayerSwitchWeapon() {
    for (int step = 1; step < NUM_WEAPONS; step++) {
        int next = m_curWeapon + step;
        if (next >= NUM_WEAPONS) {
            next -= NUM_WEAPONS;
        }
        if (m_weapons[next] != NULL && m_weapons[next]->IsSelectable()) {
            return true;
        }
    }
    return false;
}

// Hit by a bullet (or by a rocket or a grenade that explodes on impact):
// script event 0x2B with the damage, the HUD's hit direction, rumble and an
// impact sound. Multiplayer: hit-location damage, team damage off unless
// friendly fire, shooter statistics; grenades explode.
void CPlayerObject::HandleBulletCollision(CBullet* bullet, const CCollision& clsn) {
    if (m_dead) {
        return;
    }
    CProjectileBullet* projectile = bullet->AsProjectile();
    bool impact = false;
    CThrownBullet* thrown = bullet->AsThrown();
    if (thrown != NULL) {
        bool hits = false;
        if (thrown->m_type == 5 || thrown->m_type == 9) {
            hits = true;
        }
        impact = hits;
    }
    if (projectile != NULL || impact) {
        ISceneNode* firedBy = bullet->GetFiredBy();
        BS_STRUCT_HitByBulletInfo_struct* event =
            (BS_STRUCT_HitByBulletInfo_struct*)BSEventGetEventMemoryBlock(sizeof(BS_STRUCT_HitByBulletInfo_struct));
        event->m_bulletType = bullet->GetScriptBulletType();
        event->m_damage = bullet->GetDamage();
        if (firedBy != NULL) {
            event->m_firedBy = firedBy->GetScriptObject();
        } else {
            event->m_firedBy = NULL;
        }
        CVector3 dir;
        CVector3 forward;
        CVector3 right;
        bullet->GetForward(dir);
        GetForward(forward);
        GetRightward(right);
        float alongRight = right.z * dir.z + (right.x * dir.x + right.y * dir.y);
        if (alongRight < -0.7071f) {
            m_ui.AddHitDirection(2);
            event->m_side = 3;
        } else if (alongRight > 0.7071f) {
            m_ui.AddHitDirection(4);
            event->m_side = 2;
        } else if (forward.z * dir.z + (forward.x * dir.x + forward.y * dir.y) < 0.0f) {
            m_ui.AddHitDirection(1);
            event->m_side = 0;
        } else {
            m_ui.AddHitDirection(3);
            event->m_side = 1;
        }
        if (g_cheats.m_invincible) {
            event->m_damage = 0;
        } else if (g_cheats.field_0xE) {
            event->m_damage = 1000;
        }
        if (g_bInMultiplayerMode) {
            CPlayerObject* shooter = firedBy->AsPlayerObject();
            if (g_Shell.field_0x1434 == 0 && shooter->m_team == m_team) {
                event->m_damage = 0;
            }
            event->m_pPoly = clsn.m_pContacts->m_pPoly;
            int location = (int)event->m_pPoly;
            if (location < 9) {
                if (location == 2) {
                    event->m_damage = (float)event->m_damage;
                } else if (location < 2) {
                    if (location >= 0) {
                        event->m_damage = 1.0f * (3.0f * event->m_damage);
                    }
                } else {
                    event->m_damage = 0.5f * event->m_damage;
                }
            } else if (location < 21) {
                if (location < 13) {
                    event->m_damage = (float)event->m_damage;
                } else {
                    event->m_damage = 0.5f * event->m_damage;
                }
            }
            CPlayerObject* attacker = event->m_firedBy->m_gameObject->GetSceneNode()->AsPlayerObject();
            if (event->m_damage != 0 && *(int*)m_pScriptObject->m_gameObject->GetScriptData() > 0) {
                g_Shell.Add_MPShotLocation(attacker->m_playerIndex, m_playerIndex, 1,
                                           ShotLocation((int)event->m_pPoly));
            }
        }
        BSObjectTriggerEvent(m_pScriptObject, 0x2B, event, NULL, false);
        g_inputMgr.SetFeedback(m_controllerPort, 0xC, 0,
                               MathFunClamp<float>((float)event->m_damage / 20.0f, 0.0f, 1.0f), 0.1f);
        CVector3 pos;
        bullet->GetPosition(pos);
        int sound = 1;
        if (thrown != NULL) {
            if (thrown->m_type == 5) {
                sound = 5;
            } else {
                sound = 2;
            }
        }
        PlayImpactSound(0xE, sound, pos, true);
        if (thrown != NULL && thrown->m_type == 5) {
            thrown->MarkForDestruction(0);
        }
    }
    if (g_bInMultiplayerMode && thrown != NULL) {
        thrown->Explode();
    }
}

// Touching a pickup takes it; every contact is collected for
// ConstrainVelocity, and world contacts give the footstep surface.
void CPlayerObject::OnCollision(const CCollision& clsn) {
    bool first = clsn.m_pNode0 == this;
    ISceneNode* other;
    if (first) {
        other = clsn.m_pNode1;
    } else {
        other = clsn.m_pNode0;
    }
    if (other->GetCollisionId() == 0xE) {
        int health = 0;
        CPlayerWeaponObject* weapon = NULL;
        int amount;
        int crc;
        int weaponType;
        EDumpType type;
        void* pickup = HitDumpObject((CStaticObject*)other, &amount, &crc, &type, &weaponType);
        switch ((u32)crc) {
        case 0x598E4105:
        case 0x72441AA1:
            health = 10;
            break;
        case 0x10EFEBA2:
            health = 25;
            break;
        case 0xF7BADDB9:
            health = 50;
            break;
        default:
            weapon = m_weapons[GetWeaponInfoByCRC(crc, NULL, NULL)];
            if (weapon == NULL) {
                DebugMsg("What the heck are we trying to pick up?  crc = 0x%x\n", crc);
                other->SetCollisionId((EClsnId)-1);
            }
            break;
        }
        if (weapon != NULL) {
            AddAmmoOrWeapon(crc, amount, pickup);
        }
        if (health > 0) {
            AddHealth(crc, health, pickup);
        }
    }
    if (other->AsBullet() != NULL) {
        return;
    }
    for (const SClsnContact* c = clsn.m_pContacts; c != NULL; c = c->m_pNext) {
        CClsnInfo info;
        info.m_pOther = other->AsAnimObject();
        info.m_depth = c->m_depth;
        if (first) {
            info.m_normal.x = -c->m_normal.x;
            info.m_normal.y = -c->m_normal.y;
            info.m_normal.z = -c->m_normal.z;
        } else {
            info.m_normal = c->m_normal;
        }
        m_contacts.push_back(info);
        if (c->m_normal.z > 0.642f) {
            m_onGround = 1;
        }
        if (other->AsWorldObject() != NULL && c->m_normal.z > m_groundZ) {
            m_groundZ = c->m_normal.z;
            const SClsnPoly* poly = c->m_pPoly;
            if (poly != NULL && (s16)poly->m_surfaceType >= 0) {
                m_surfaceSound = CSurfaceType::GetSoundIndex(poly->m_surfaceType);
            } else {
                m_surfaceSound = 1;
            }
        }
    }
    m_collided = 1;
}

// Multiplayer: the body follows the player (just its position when dead).
static inline void PositionBody(CPlayerObject* player) {
    if (player->m_dead) {
        CVector3 pos(*(CVector3*)player->m_tm.m[3]);
        pos.z = pos.z + g_MPAnimMeshZOffset;
        player->m_pBody->SetPosition(pos);
    } else {
        CMatrix tm;
        tm = player->m_tm;
        tm.Translate(CVector3(0.0f, 0.0f, g_MPAnimMeshZOffset));
        player->m_pBody->SetTMLocalToWorld(tm);
    }
}

// Moves by the velocity (from where the last frame ended), sizes the body
// capsule to the eye height and resets this frame's contacts.
int CPlayerObject::AttemptUpdate(float dt) {
    if (g_bInMultiplayerMode) {
        PositionBody(this);
    }
    m_dt = dt;
    CVector3 move;
    move.x = dt * m_velocity.x;
    move.y = dt * m_velocity.y;
    move.z = dt * m_velocity.z;
    if (!m_onMG && !m_attached) {
        m_tm.BuildRotZ(m_yaw);
        m_tm.SetPos(field_0x288);
        m_tm.Translate(move);
    }
    float eye = (m_eyeHeightRate * dt) + m_eyeHeight;
    float crouch = g_bInMultiplayerMode ? EYE_CROUCH_MP : EYE_CROUCH_SP;
    if (eye > EYE_STAND) {
        eye = EYE_STAND;
        m_crouchMoving = 0;
    } else if (eye < crouch) {
        eye = crouch;
        m_crouchMoving = 0;
    }
    CLine3 axis(CVector3(0.0f, 0.0f, 0.0f), CVector3(0.0f, 0.0f, eye));
    if (&axis != &m_localCapsule.m_line) {
        m_localCapsule.m_line = axis;
    }
    m_worldCapsule.TransformedCopy(m_localCapsule, m_tm);
    CVector3 min;
    CVector3 max;
    m_worldCapsule.GetExtents(min, max);
    m_pMinX->m_value = min.x - CONTACT_EPSILON;
    m_pMinY->m_value = min.y - CONTACT_EPSILON;
    m_pMaxX->m_value = CONTACT_EPSILON + max.x;
    m_pMaxY->m_value = CONTACT_EPSILON + max.y;
    m_onGround = 0;
    m_onSlope = 0;
    m_collided = 0;
    if (!m_onMG) {
        m_contacts.m_size = 0;
    }
    m_groundZ = 0.0f;
}

// Multiplayer: the body's walk animation (speed and direction). Single
// player: a player in the void (nothing under it) stops. Places the camera,
// resolves the contacts, and fires the mounted gun's effects (flash
// particles, statistics, ejected casings).
int CPlayerObject::CommitUpdate() {
    if (g_bInMultiplayerMode) {
        PositionBody(this);
        CVector3 d(*(CVector3*)m_tm.m[3]);
        d.x = d.x - field_0x288.x;
        d.y -= field_0x288.y;
        d.z = d.z - field_0x288.z;
        d.z = 0.0f;
        float speed = sqrtf(d.x * d.x + d.y * d.y + 0.0f * 0.0f);
        if (speed != 0.0f) {
            float inv = 1.0f / speed;
            d.x = d.x * inv;
            d.y *= inv;
            d.z = d.z * inv;
        }
        int dir = 0;
        if (speed > 1.1920929e-7f) {
            CVector3 fwd(*(CVector3*)m_tm.m[1]);
            fwd.Normalize();
            float dot = fwd.z * d.z + (fwd.x * d.x + fwd.y * d.y);
            if (dot < -0.7071f) {
                dir = 3;
            } else if (dot < 0.7071f) {
                // (The original's side test always picks 1.)
                fwd.z = 0.0f;
                d.z = 0.0f;
                fwd.x = fwd.y * 0.0f - 0.0f * d.y;
                fwd.y = 0.0f * d.x - fwd.x * 0.0f;
                fwd.z = fwd.x * d.y - fwd.y * d.x;
                if (fwd.z > 0.0f) {
                    dir = 2;
                } else {
                    dir = 1;
                }
            }
        }
        float rate;
        long state;
        m_pBody->GetFlocoParams(rate, state);
        m_pBody->SetFlocoParams(speed + rate, dir);
    }
    if (!m_attached) {
        const IVolume* world = g_scene.m_world->GetWorldBoundingVolume((ISceneNode::EVolumeType)0);
        CVector3 pos(*(CVector3*)m_tm.m[3]);
        CVector3 top(pos);
        top.z = top.z + 0.4f;
        CVector3 bottom(pos.x, pos.y, -1e30f);
        CLine3 line(top, bottom);
        CVolFiber below(line);
        CCollision clsn;
        CCDBObject::DisablePassThru(true);
        int result = world->TestCollision(below, clsn, false);
        CCDBObject::DisablePassThru(false);
        static bool toggle = true;
        if (result == CLSN_NONE) {
            // Out of the level: stop.
            if (toggle) {
                toggle = false;
            }
            m_localVelocity.z = 0.0f;
            m_localVelocity.y = 0.0f;
            m_localVelocity.x = 0.0f;
            m_velocity.z = 0.0f;
            m_velocity.y = 0.0f;
            m_velocity.x = 0.0f;
            m_onGround = 1;
            AttemptUpdate(0.0f);
            return;
        }
        toggle = true;
    } else {
        if (m_attachedKeepWeapon) {
            m_tm.BuildRotZ(m_yaw);
            m_tm.SetPos(field_0x740);
        }
        CMatrix tm;
        tm = m_tm;
        CMatrix cam;
        cam.Multiply(m_eyeTransform, tm);
        m_camera.SetTMLocalToWorld(cam);
        float fov = 0.017453292f * m_fov;
        m_camera.SetPerspective(fov, fov * CCamera::ASPECT_4x3);
        m_camera.AttemptUpdate(1.0f);
        CPlayerWeaponObject* weapon;
        if (m_onMG) {
            weapon = NULL;
        } else if (m_curWeapon >= 0) {
            weapon = m_weapons[m_curWeapon];
        } else {
            weapon = NULL;
        }
        if (!m_hideWeapon && weapon != NULL) {
            weapon->UpdateWeaponTransforms();
        }
    }
    ConstrainVelocity();
    field_0x288 = *(CVector3*)m_tm.m[3];
    m_eyeHeight = m_localCapsule.m_line.m_end.z;
    if (m_pMGWeapon == NULL) {
        return;
    }
    m_pMGWeapon->Update(m_dt, !m_onMG);
    if (!m_pMGWeapon->m_fired) {
        return;
    }
    if (m_pMGWeapon != m_pMGCannon) {
        CParticleSystem* ps = CParticleSystem::Create(0x2C0C51FB);
        if (ps != NULL) {
            CMatrix flash;
            g_pStaticObjectMuzzleFlashPlayer->GetTMLocalToWorld(flash);
            ps->SetLocalToWorld(flash);
        }
    }
    g_Shell.Set_shotsFired(1, false);
    if (!m_mgEjects) {
        return;
    }
    CThrownObject* casing = CreateSpecialThrownObject(NULL, 0x9AB689E6, -1);
    if (casing == NULL) {
        return;
    }
    casing->SetCollisionId((EClsnId)-1);
    CMatrix tm;
    m_pMG->GetTMLocalToWorld(tm);
    tm.Multiply(m_mgEjectTM, tm);
    float rx = MathFunRandomReal(-0.5f, 0.5f);
    float ry = MathFunRandomReal(-0.5f, 0.5f);
    float rz = MathFunRandomReal(-0.5f, 0.5f);
    float vz = 0.016683351f * MathFunRandomReal(1.0f, 2.0f);
    float vy = 0.016683351f * MathFunRandomReal(0.0f, 0.5f);
    CVector3 vel;
    vel.x = 0.016683351f * MathFunRandomReal(2.0f, 2.5f);
    vel.y = vy;
    vel.z = vz;
    tm.TransformVector(vel, vel);
    casing->SetPosition(*(CVector3*)tm.m[3]);
    casing->SetVelocity(vel);
    CMatrix spin;
    spin = tm;
    spin.RotateX(rx);
    spin.RotateY(ry);
    spin.RotateZ(rz);
    casing->m_spin.SetFromMatrix(spin);
    casing->BeginUpdate(1.0f);
}

// Slides the velocity along this frame's contacts: off the plane it pushes
// into most, then along the crease with the next one, or stops in a corner.
// Standing up under a low ceiling crouches again.
void CPlayerObject::ConstrainVelocity() {
    if (m_contacts.m_size == 0) {
        return;
    }
    if ((float)fabs(m_velocity.z * m_velocity.z + (m_velocity.x * m_velocity.x + m_velocity.y * m_velocity.y)) <
            0.000001f &&
        m_eyeHeightRate <= 0.0f) {
        m_velocity.z = 0.0f;
        m_velocity.y = 0.0f;
        m_velocity.x = 0.0f;
        m_localVelocity.z = 0.0f;
        m_localVelocity.y = 0.0f;
        m_localVelocity.x = 0.0f;
        return;
    }
    if (m_onMG) {
        return;
    }
    bool changed = false;
    CClsnInfo* begin = m_contacts.m_data;
    CVector3 v0(m_velocity);
    float minDot = 0.0f;
    CClsnInfo* end = begin + m_contacts.m_size;
    float minDot2 = 0.0f;
    m_onSlope = 1;
    CClsnInfo* first = end;
    CClsnInfo* second = end;
    for (CClsnInfo* c = begin; c != end; c++) {
        if (c->m_pOther != NULL) {
            c->m_pOther->ConstrainVelocity();
        }
        CVector3 n(c->m_normal);
        float d = v0.z * n.z + (v0.x * n.x + v0.y * n.y);
        if (d < minDot) {
            minDot = d;
            first = c;
        }
        bool slope = false;
        if (c->m_normal.z <= 0.642f && c->m_normal.z > 0.01f) {
            slope = true;
        }
        m_onSlope = m_onSlope & slope;
        if (m_eyeHeightRate > 0.0f && c->m_normal.z < 0.0f) {
            // Standing up into something.
            if (m_crouchMoving && m_canCrouch) {
                float depth = c->m_depth;
                if (depth > 0.0f) {
                    depth = 0.0f;
                }
                CVector3 dir(v0);
                dir.Normalize();
                float nz = c->m_normal.z;
                float limit = -0.28f * ((CONTACT_EPSILON + depth) / CONTACT_EPSILON) *
                              ((0.5f * (dir.z * nz + (dir.x * c->m_normal.x + dir.y * c->m_normal.y))) + 0.5f);
                if (limit > -0.0f) {
                    limit = -0.0f;
                } else if (limit < -0.28f) {
                    limit = -0.28f;
                }
                if (nz < limit && m_canCrouch) {
                    m_crouched = 1;
                    m_crouchMoving = 1;
                    m_eyeHeightRate = -(g_bInMultiplayerMode ? 0.02666667f : 0.07333334f);
                }
            } else if (c->m_normal.z < -0.0f) {
                m_eyeHeightRate = 0.0f;
            }
        }
    }
    bool ok = first != end;
    if (ok) {
        float k = 1.00001f * minDot;
        m_velocity.x = v0.x - k * first->m_normal.x;
        m_velocity.y = v0.y - k * first->m_normal.y;
        m_velocity.z = v0.z - k * first->m_normal.z;
        float len = m_velocity.Length();
        changed = true;
        float threshold = -0.0034906515f * len;
        for (CClsnInfo* c = begin; c != end; c++) {
            if (c == first) {
                continue;
            }
            CVector3 n(c->m_normal);
            float d = m_velocity.z * n.z + (m_velocity.x * n.x + m_velocity.y * n.y);
            if (d < threshold && d < minDot2) {
                minDot2 = d;
                second = c;
            }
        }
        ok = second != end;
        if (ok) {
            CVector3 edge;
            edge.Cross(first->m_normal, second->m_normal);
            float elen = edge.Length();
            if (elen != 0.0f) {
                float inv = 1.0f / elen;
                edge.x *= inv;
                edge.y *= inv;
                edge.z *= inv;
            }
            ok = elen > 0.0034906515f;
            if (ok) {
                float along = edge.z * v0.z + (edge.x * v0.x + edge.y * v0.y);
                m_velocity.x = along * edge.x;
                m_velocity.y = along * edge.y;
                m_velocity.z = along * edge.z;
                len = m_velocity.Length();
                changed = true;
                threshold = -0.0034906515f * len;
                for (CClsnInfo* c = begin; ok && c != end; c++) {
                    if (c == first || c == second) {
                        continue;
                    }
                    CVector3 n(c->m_normal);
                    if (m_velocity.z * n.z + (m_velocity.x * n.x + m_velocity.y * n.y) < threshold) {
                        changed = true;
                        ok = false;
                        m_velocity.z = 0.0f;
                        m_velocity.y = 0.0f;
                        m_velocity.x = 0.0f;
                    }
                }
            }
        }
    }
    if (changed) {
        CMatrix inv;
        inv.FastInverse(m_tm);
        inv.TransformVector(m_localVelocity, m_velocity);
    }
}

// The point of `path` at `t` (0..1 over its segments).
static inline void SplineExpand(CAISplinePath* path, float t, CVector3& out) {
    float f = t * (float)(int)path->m_lastSegment;
    int i = (int)f;
    path->m_segments[i].Expand(f - (float)i, out);
}

// MoveOnPath: moves along the path (the velocity takes the player there)
// and looks along the look path (or the path's direction).
void CPlayerObject::UpdateMovePath(float dt) {
    if (!m_onPath) {
        return;
    }
    float t = (0.5f * m_pathRate * dt) + m_pathPos;
    m_pathPos = (m_pathRate * dt) + m_pathPos;
    if (t < 0.0f) {
        t = 0.0f;
    }
    if (t > 1.0f) {
        t = 1.0f;
        m_onPath = 0;
    }
    CVector3 pos;
    if (m_pPath != NULL) {
        SplineExpand(m_pPath, t, pos);
    } else {
        pos.x = field_0x288.x + (dt * m_velocity.x);
        pos.y = field_0x288.y + (dt * m_velocity.y);
        pos.z = field_0x288.z + (dt * m_velocity.z);
    }
    float inv = 1.0f / dt;
    m_velocity.x = pos.x - field_0x288.x;
    m_velocity.y = pos.y - field_0x288.y;
    m_velocity.z = pos.z - field_0x288.z;
    m_velocity.x = m_velocity.x * inv;
    m_velocity.y = m_velocity.y * inv;
    m_velocity.z = m_velocity.z * inv;
    if (m_pLookPath == NULL) {
        return;
    }
    CVector3 fwd;
    if (m_pLookPath == m_pPath) {
        float f = t * (float)(int)m_pPath->m_lastSegment;
        int i = (int)f;
        m_pPath->m_segments[i].ExpandDerivative(f - (float)i, fwd);
        fwd.Normalize();
    } else {
        CVector3 look;
        SplineExpand(m_pLookPath, t, look);
        fwd.x = look.x - pos.x;
        fwd.y = look.y - pos.y;
        fwd.z = look.z - pos.z;
        fwd.Normalize();
    }
    CVector3 right;
    right.Cross(fwd, CVector3(0.0f, 0.0f, 1.0f));
    right.Normalize();
    CVector3 up;
    up.Cross(right, fwd);
    up.Normalize();
    CMatrix m;
    m.SetRight(right);
    m.SetUp(up);
    m.SetFront(fwd);
    float roll;
    m.ToEulerXYZ(m_pitch, roll, m_yaw);
}

// The mounted gun crosshair (built once, in place).
static CSprite* g_pMGCrossHair;
static u32 g_pMGCrossHairMem[0x64 / 4];
static int g_cScreenWidth;
static int g_cScreenHeight;

// The standing body: capsule from the feet to the eyes, bounding box.
static inline void SetStandingVolumes(CPlayerObject* player) {
    player->m_localCapsule.m_radius = 0.4f;
    player->m_eyeHeight = EYE_STAND;
    CLine3 axis(CVector3(0.0f, 0.0f, 0.0f), CVector3(0.0f, 0.0f, player->m_eyeHeight));
    if (&axis != &player->m_localCapsule.m_line) {
        player->m_localCapsule.m_line = axis;
    }
}

static inline void SetBoundingBox(CPlayerObject* player) {
    player->m_localBox.SetBasis(CVector3(1.0f, 0.0f, 0.0f), CVector3(0.0f, 1.0f, 0.0f), CVector3(0.0f, 0.0f, 1.0f));
    player->m_localBox.SetWidth(0.75f);
    player->m_localBox.SetHeight(1.5f);
    player->m_localBox.SetDepth(0.6f);
    player->m_localBox.SetCenter(CVector3(0.25f, 0.7f, -0.1f));
}

CPlayerObject::~CPlayerObject() {
    for (int i = 0; i < NUM_WEAPONS; i++) {
        CPlayerWeaponObject* weapon = m_weapons[i];
        if (weapon != NULL) {
            weapon->Shutdown();
            g_AnimObjFactory.ReleaseObject(weapon);
        }
    }
}

// Multiplayer: the name, team, the soldier body for the chosen character
// and the level's weapon set (with `reset`, the ammo starts from scratch).
void CPlayerObject::InitMultiplayer(MPPlayerData* data, bool reset) {
    strncpy(m_name, data->m_name, 0x13);
    int i = 0x13;
    do {
        m_name[i] = 0;
        i--;
    } while (i >= 0 && (m_name[i] == ' ' || m_name[i] == 0));
    m_team = data->m_team;
    char sleeve[0x10];
    if (m_pBody == NULL) {
        u32 crc;
        switch (data->m_character) {
        case 0:
            crc = 0x6400F245;
            strcpy(sleeve, "GI_SLEEVE_TAN");
            break;
        case 1:
            crc = 0x984C0C7C;
            strcpy(sleeve, "GI_SLEEVE_GREEN");
            break;
        case 2:
            crc = 0x93EAB2B9;
            strcpy(sleeve, "GI_SLEEVE_TAN");
            break;
        case 3:
            crc = 0x56CFD653;
            strcpy(sleeve, "GI_SLEEVE_TAN");
            break;
        case 4:
            crc = 0x4477EECA;
            strcpy(sleeve, "GI_SLEEVE_TAN");
            break;
        case 5:
            crc = 0x4FBD6E03;
            strcpy(sleeve, "GI_SLEEVE_BLUE");
            break;
        case 6:
            crc = 0x961A11A8;
            strcpy(sleeve, "GI_SLEEVE_TAN");
            break;
        case 7:
            crc = 0xB155881B;
            strcpy(sleeve, "GI_SLEEVE_GREY");
            break;
        case 8:
            crc = 0x1EAF7765;
            strcpy(sleeve, "GI_SLEEVE_TAN");
            break;
        case 9:
            crc = 0xE9FCAD04;
            strcpy(sleeve, "GI_SLEEVE_BLACK");
            break;
        case 10:
            crc = 0xAFBCE1DF;
            strcpy(sleeve, "GI_SLEEVE_GREY");
            break;
        case 11:
            crc = 0x3E35BB3E;
            strcpy(sleeve, "GI_SLEEVE_WHITE");
            break;
        case 12:
            crc = 0xD924F993;
            strcpy(sleeve, "GI_SLEEVE_TAN");
            break;
        case 13:
            crc = 0xCE9664A6;
            strcpy(sleeve, "GI_SLEEVE_BLUE");
            break;
        case 14:
            crc = 0x81B474A3;
            strcpy(sleeve, "GI_SLEEVE_GREEN");
            break;
        case 15:
            crc = 0x9FB1FF81;
            strcpy(sleeve, "GI_SLEEVE_BLUE");
            break;
        case 16:
            crc = 0x8FAB68CE;
            strcpy(sleeve, "GI_SLEEVE_GREY");
            break;
        case 17:
            crc = 0x410C8385;
            strcpy(sleeve, "GI_SLEEVE_GREEN");
            break;
        case 18:
            crc = 0x7B8BF438;
            strcpy(sleeve, "GI_SLEEVE_WHITE");
            break;
        case 19:
            crc = 0x6ABC144B;
            strcpy(sleeve, "GI_SLEEVE_TAN");
            break;
        default:
            crc = 0x410C8385;
            strcpy(sleeve, "GI_SLEEVE_GREEN");
            break;
        }
        m_pBody = (CAnimatedPlayer*)g_AnimObjFactory.AllocateObject(crc, (EObjectClass)OBJCLASS_ANIMATED_PLAYER);
        m_pBody->SetScript(m_pScriptObject);
        m_pBody->m_pPlayer = this;
        g_scene.Add(*m_pBody);
    }
    // The weapon set: count, then per weapon its flag, ammo, CRC and the
    // body's weapon CRC.
    LevelFileContentsStruct_* set = TLT_FindNextResourceByTypeAndVal((TLTResourceID)0x16, 0, g_levelWeaponSetID, NULL);
    int count = set->fields[1].value;
    for (int w = 0; w < count; w++) {
        int crc = set->fields[20 + w].value;
        int bodyCrc = set->fields[29 + w].value;
        int type = GetWeaponInfoByCRC(crc, NULL, NULL);
        CPlayerWeaponObject** slot = &m_weapons[type];
        if (*slot == NULL) {
            *slot = (CPlayerWeaponObject*)g_AnimObjFactory.AllocateObject(crc, (EObjectClass)OBJCLASS_PLAYER_WEAPON);
            (*slot)->Init((EWeaponTypes)type, 0, this);
            (*slot)->m_selectable = 0;
            (*slot)->SetLocalBoundingVolume((ISceneNode::EVolumeType)3, &m_localBox);
            (*slot)->SetWorldBoundingVolume((ISceneNode::EVolumeType)3, &m_worldBox);
            (*slot)->SetScript(m_pScriptObject);
            (*slot)->SetSleeveTexture(sleeve);
            m_pBody->AddWeapon(w, bodyCrc);
            m_bodyWeaponSlot[type] = w;
        }
    }
    for (int w = 0; w < count; w++) {
        if (set->fields[2 + w].value != 0) {
            int type = GetWeaponInfoByCRC(set->fields[20 + w].value, NULL, NULL);
            if (reset) {
                m_weapons[type]->m_pWeapon->Empty();
            }
            m_weapons[type]->AddAmo(set->fields[11 + w].value);
            m_weapons[type]->m_selectable = 1;
            m_weapons[type]->DoneReloading();
        }
    }
    TriggerScriptEvent(0xA2, NULL, false);
    CycleWeapon(1, -1);
}

// Multiplayer: back to full health and the starting weapons, standing.
void CPlayerObject::Respawn() {
    if (g_bMPEndLevel) {
        return;
    }
    BSObject* script = GetScriptObject();
    if (script != NULL) {
        TriggerObjectData_struct* data = script->m_trigger->m_data;
        int* health = (int*)script->m_gameObject->GetScriptData();
        if (g_bInMultiplayerMode) {
            *health = g_Shell.field_0x143C[m_controllerPort];
        } else if (data != NULL) {
            *health = data->field_0x70;
        } else {
            *health = 0xFF;
        }
    }
    m_mgTrigger = -1;
    m_collisionId = 1;
    m_pLocalBound = NULL;
    m_pWorldBound = NULL;
    SetStandingVolumes(this);
    SetBoundingBox(this);
    m_pLastMG = NULL;
    m_curWeapon = -1;
    m_cookTime = -1.0f;
    m_grenadeHoldTime = -1.0f;
    m_fireDelay = RESPAWN_FIRE_DELAY;
    m_wasZoomed = 0;
    m_mgTurning = 0;
    m_crouched = 0;
    m_crouchMoving = 0;
    m_canCrouch = 1;
    m_motionShakeOn = 0;
    m_cameraShake = 0;
    m_onPath = 0;
    m_climbing = 0;
    m_falling = 0;
    m_fallStart = 0.0f;
    m_onMG = 0;
    m_mounting = 0;
    m_gravity = 1;
    m_attachedKeepWeapon = 1;
    m_attached = 0;
    m_hideWeapon = 0;
    m_wielding = 0;
    m_fallingDamage = 1;
    m_stepReady = 1;
    m_dt = 1.0f;
    m_stepTimer = 0.0f;
    m_pMGWeapon = NULL;
    m_triggerChecks = 1;
    m_superAim = 0;
    m_moveState = 0;
    m_recoilOffset = 0.0f;
    m_lastPitch = 0.0f;
    m_recoilKick = 0.0f;
    field_0x7C0 = 0.0f;
    field_0x7C4 = 0;
    m_fov = 35.0f;
    m_scopeFov = 35.0f;
    m_scopeView = 0;
    m_fovRate = 0.0f;
    m_peekOrigin = CVector3(0.0f, 0.0f, 0.0f);
    field_0x44C = 0.0f;
    field_0x450 = 0.0f;
    field_0x78 = 0.0f;
    field_0x7C = 0.0f;
    m_recoilYaw = 0.0f;
    m_recoilPitch = 0.0f;
    m_surfaceSound = 1;
    m_landingTimer = 0;
    m_peeking = 0;
    m_pushed = 0;
    m_lastFire = 0.0f;
    m_dead = 0;
    field_0x760 = -1;
    m_pitch = 0.0f;
    g_inputMgr.m_feedbackBlocked[field_0x768] = 0;
    InitMultiplayer((MPPlayerData*)&g_Shell.m_players[m_controllerPort], true);
    RestartPlayer(m_playerIndex);
}

// The player's weapons (single player: every player weapon of the level,
// with the ammo carried over from the last level), mounted gun weapons,
// HUD, volumes and the starting state.
int CPlayerObject::Init() {
    m_ui.Init();
    m_mgTrigger = -1;
    m_collisionId = 1;
    m_pLocalBound = NULL;
    m_pWorldBound = NULL;
    m_pBody = NULL;
    SetStandingVolumes(this);
    Reset();
    SetBoundingBox(this);
    if (!g_bInMultiplayerMode) {
        for (LevelFileContentsStruct_* res = TLT_FindNextResourceByType((TLTResourceID)0x13, NULL); res != NULL;
             res = TLT_FindNextResourceByType((TLTResourceID)0x13, res)) {
            int crc = res->fields[7].value;
            int type = GetWeaponInfoByCRC(crc, NULL, NULL);
            if (m_weapons[type] == NULL) {
                m_weapons[type] = (CPlayerWeaponObject*)g_AnimObjFactory.AllocateObject(crc, (EObjectClass)OBJCLASS_PLAYER_WEAPON);
                m_weapons[type]->Init((EWeaponTypes)type, 0, this);
                m_weapons[type]->m_selectable = 0;
                m_weapons[type]->SetLocalBoundingVolume((ISceneNode::EVolumeType)3, &m_localBox);
                m_weapons[type]->SetWorldBoundingVolume((ISceneNode::EVolumeType)3, &m_worldBox);
            }
        }
        int mission;
        int stage;
        if (g_bInMultiplayerMode) {
            mission = g_Shell.m_mpMission;
            stage = g_Shell.m_mpStage;
        } else {
            mission = g_Shell.Get_currentMission();
            stage = g_Shell.Get_currentStage();
        }
        if ((u8)g_Shell.ShouldIUsePlayersFinishedLevelData(mission, stage)) {
            for (int i = 0; i < NUM_WEAPONS; i++) {
                if (m_weapons[i] != NULL) {
                    m_weapons[i]->AddAmo(g_Shell.GetPlayersFinishedLevelAmmo(mission, stage - 1, (EWeaponTypes)i));
                }
            }
        }
    }
    m_pMGGun = g_WeaponFactory.CreateWeapon((EWeaponTypes)0x1B, this, 999);
    m_pMGGun->SetInfiniteAmo(true);
    m_pMGGun2 = g_WeaponFactory.CreateWeapon((EWeaponTypes)0x2B, this, 999);
    m_pMGGun2->SetInfiniteAmo(true);
    m_pMGCannon = g_WeaponFactory.CreateWeapon((EWeaponTypes)0x27, this, 999);
    m_pMGCannon->SetInfiniteAmo(true);
    LevelFileContentsStruct_* level = TLT_FindNextResourceByType((TLTResourceID)0xB, NULL);
    m_mgFlashScale.x = *(f32*)&level->fields[59].value;
    m_mgFlashScale.y = *(f32*)&level->fields[60].value;
    m_mgFlashScale.z = *(f32*)&level->fields[61].value;
    g_fScriptExplosionVelocityScaleOnPlayer = *(f32*)&level->fields[63].value;
    g_fScriptExplosionHeightScaleOnPlayer = *(f32*)&level->fields[64].value;
    m_pLastMG = NULL;
    m_curWeapon = -1;
    m_cookTime = -1.0f;
    m_grenadeHoldTime = -1.0f;
    m_wasZoomed = 0;
    m_mgTurning = 0;
    m_hearing.Init(this, (EClsnId)8, 3.0f, 0.5f, PlayBulletWhizBys);
    g_cScreenWidth = g_pScreen->GetWidth();
    g_cScreenHeight = g_pScreen->GetHeight();
    m_pScriptObject = NULL;
    m_crouched = 0;
    m_jumping = 0;
    m_canCrouch = 1;
    m_motionShakeOn = 0;
    m_cameraShake = 0;
    m_onPath = 0;
    m_climbing = 0;
    m_falling = 0;
    m_fallStart = 0.0f;
    m_onMG = 0;
    m_mounting = 0;
    m_gravity = 1;
    m_attachedKeepWeapon = 1;
    m_attached = 0;
    m_hideWeapon = 0;
    m_wielding = 0;
    m_fallingDamage = 1;
    m_stepReady = 1;
    m_dt = 1.0f;
    m_stepTimer = 0.0f;
    m_pMGWeapon = NULL;
    m_triggerChecks = 1;
    m_superAim = 0;
    m_fireDelay = 0;
    m_moveState = 0;
    m_recoilOffset = 0.0f;
    m_lastPitch = 0.0f;
    m_recoilKick = 0.0f;
    field_0x7C0 = 0.0f;
    field_0x7C4 = 0;
    m_fov = 35.0f;
    m_scopeFov = 35.0f;
    m_scopeView = 0;
    m_fovRate = 0.0f;
    m_peekOrigin = CVector3(0.0f, 0.0f, 0.0f);
    field_0x44C = 0.0f;
    field_0x450 = 0.0f;
    field_0x78 = 0.0f;
    field_0x7C = 0.0f;
    m_recoilYaw = 0.0f;
    m_recoilPitch = 0.0f;
    m_surfaceSound = 1;
    m_landingTimer = 0;
    m_peeking = 0;
    LevelFileContentsStruct_* textures = TLT_FindNextResourceByType((TLTResourceID)0xD, NULL);
    if (textures->fields[57].value != 0) {
        StartPlayerCrouched();
    }
    float cannon = *(f32*)&textures->fields[59].value;
    if (cannon > 0.0f && m_pMGCannon != NULL) {
        m_pMGCannon->m_speedScale = cannon;
    }
    m_pushed = 0;
    m_lastFire = 0.0f;
    m_dead = 0;
    if (g_pMGCrossHair == NULL) {
        LevelFileContentsStruct_* res = TLT_FindNextResourceByType((TLTResourceID)0xB, NULL);
        g_pMGCrossHair = ::new (&g_pMGCrossHairMem) CSprite((char*)res->fields[29].value, (CSprite::ESpriteLoadType)1, (ERenderPriority)0xA);
    }
    field_0x6F4 = g_pMGCrossHair;
    if (m_pMuzzleFlashSprite == NULL) {
        m_pMuzzleFlashSprite = new CSprite("data\\bitmaps\\MuzzleFlash.gsh", (CSprite::ESpriteLoadType)1, (ERenderPriority)7);
    }
    field_0x760 = -1;
    m_playerIndex = 0;
    field_0x768 = 0;
    field_0x1298 = 0;
}

CPlayerObject::CPlayerObject() : m_contacts(-1) {
    ClearSceneNodeData();
    m_pMuzzleFlashSprite = NULL;
    field_0x6F4 = NULL;
    m_contacts.allocate(MAX_CONTACTS);
    m_bsgo.m_player = NULL;
    m_bsgo.m_object = NULL;
    m_bsgo.m_pNext = NULL;
    m_bsgo.m_pPrev = NULL;
}

// Mounted gun hard points.
#define HARDPOINT_MG_MUZZLE 0xE6E804D5
#define HARDPOINT_MG_PIVOT 0x48C00881
#define HARDPOINT_MG_EJECT 0x11BC0F1F
// The second machine gun's object type.
#define MG2_TYPE 0xF6798FE8

// Leaving a gun: puts the player back where it got on (moved with a turret)
// a little above the ground, out of what it touches.
static inline void PlaceAfterMG(CPlayerObject* player, CVector3& pos) {
    pos.x = pos.x + 0.0f;
    pos.y = pos.y + 0.0f;
    pos.z = pos.z + 0.01f;
    float roll;
    player->m_tm.ToEulerXYZ(player->m_pitch, roll, player->m_yaw);
    player->m_pitch = MathFunClamp<float>(player->m_pitch, -1.45f, 1.45f);
    player->m_tm.BuildRotZ(player->m_yaw);
    player->m_tm.SetPos(pos);
    CVector3 push(0.0f, 0.0f, 0.0f);
    CPlayerObject::CClsnInfo* end = player->m_contacts.m_data + player->m_contacts.m_size;
    for (CPlayerObject::CClsnInfo* c = player->m_contacts.m_data; c != end; c++) {
        push.x = push.x + c->m_normal.x;
        push.y += c->m_normal.y;
        push.z += c->m_normal.z;
    }
    push.Normalize();
    float k = 3.0f * CONTACT_EPSILON;
    CVector3 at(*(CVector3*)player->m_tm.m[3]);
    push.x = push.x * k + at.x;
    push.y = push.y * k + at.y;
    push.z = push.z * k + at.z;
    player->m_tm.SetPos(push);
    g_CameraOffset = CAMERA_STAND_OFFSET;
    player->m_eyeTransform.BuildTrans(CAMERA_STAND_OFFSET);
    player->m_eyeTransform.RotateX(player->m_pitch);
    CMatrix cam;
    cam.Multiply(player->m_eyeTransform, player->m_tm);
    player->m_camera.SetTMLocalToWorld(cam);
    player->m_camera.AttemptUpdate(1.0f);
}

// Getting on (`on`) or off the mounted gun `gun` (a turret's part with the
// pivot hard point when it belongs to a hierarchical object). The gun's
// trigger data gives the turn limits and the weapon (machine gun or
// cannon); a soldier's waypoint for the gun is marked used.
void CPlayerObject::SetUsingMountedMachineGun(bool on, CStaticObject* gun) {
    if (on && !m_onMG && !m_mounting) {
        m_pMG = gun;
        m_mgHier = gun->AsHierObject() != NULL;
        if (m_mgHier) {
            CStaticObject* part = NULL;
            CHierObject* hier = gun->AsHierObject();
            bool found = false;
            for (int i = 0; i < 0x14 && !found; i++) {
                part = hier->GetSubObject(i);
                if (part == NULL) {
                    continue;
                }
                StaticMesh* data = part->m_pMesh->m_pData;
                StaticMeshHardPoint* hp = data->m_hardPoints;
                for (int n = data->m_numHardPoints; n > 0; n--, hp++) {
                    if ((u32)hp->m_id == HARDPOINT_MG_PIVOT) {
                        found = true;
                        break;
                    }
                }
            }
            m_pMG = part;
            m_pMGHier = hier;
            m_pMGHier->m_noUpdate = 1;
            ((CHierObject*)part)->m_isMountedGun = 1;
            CMatrix parent;
            parent = ((CHierObject*)part)->m_parentTM;
            m_mgPivot = *(CVector3*)parent.m[1];
        } else {
            m_pMGHier = NULL;
        }
        CStaticMesh* mesh = m_pMG->m_pMesh;
        BSObject* script = m_pMG->GetScriptObject();
        TriggerObjectData_struct* data = script->m_trigger->m_data;
        m_mgPitchLimit = (3.1415927f * (float)data->field_0x3C) / 180.0f;
        m_mgYawLimit = (3.1415927f * (float)data->field_0x3A) / 180.0f;
        m_mgTurning = 0;
        if (data->m_flags & 2) {
            m_pMGWeapon = m_pMGCannon;
            m_mgCannon = 1;
            m_mgCannonScale = (float)data->field_0x70 / 20.0f;
        } else {
            m_mgCannon = 0;
            m_mgCannonScale = 1.0f;
            if ((u32)data->field_0x48 == MG2_TYPE) {
                m_pMGWeapon = m_pMGGun2;
            } else {
                m_pMGWeapon = m_pMGGun;
            }
        }
        m_mgRefireRate = m_pMGWeapon->m_pProperties->m_refireRate;
        if (data->field_0x71 > 1) {
            m_pMGWeapon->m_pProperties->m_refireRate = data->field_0x71;
        }
        m_mounting = 1;
        StaticMesh* meshData = mesh->m_pData;
        int n = meshData->m_numHardPoints;
        StaticMeshHardPoint* hp = meshData->m_hardPoints;
        m_mgEjects = 0;
        for (int i = 0; i < n; i++, hp++) {
            u32 id = hp->m_id;
            if (id == 0xE6E804D5) {
                m_mgMuzzleTM = hp->m_tm;
            } else if (id == HARDPOINT_MG_PIVOT) {
                m_mgPivotTM = hp->m_tm;
            } else if (id == HARDPOINT_MG_EJECT) {
                m_mgEjectTM = hp->m_tm;
                m_mgEjects = 1;
            }
        }
        m_collisionId = 0x15;
        if (!m_mgHier) {
            m_mgCollisionId = m_pMG->GetCollisionId();
            m_pMG->SetCollisionId((EClsnId)0x16);
            m_mgStartTM = m_tm;
        } else {
            // Turrets move: keep the offset from the turret.
            CVector3 offset;
            m_pMGHier->GetPosition(offset);
            CVector3 pos(*(CVector3*)m_tm.m[3]);
            offset.x = offset.x - pos.x;
            offset.y = offset.y - pos.y;
            offset.z = offset.z - pos.z;
            m_mgStartTM = m_tm;
            m_mgStartTM.SetPos(offset);
            m_mgCollisionId = m_pMGHier->GetCollisionId();
            m_pMGHier->SetCollisionId((EClsnId)0x16);
        }
        if (m_pLastMG != m_pMG) {
            m_pMGWeapon->DoneReloading();
            m_pMGWeapon->m_refireTime = 0.0f;
        }
        m_pLastMG = m_pMG;
        m_mgTrigger = SearchForClosestMachineGun(this, 10.0f);
        if (m_mgTrigger >= 0) {
            if (g_pMGTriggerObject[m_mgTrigger].m_gun == script->m_trigger) {
                if (IsMGUsed(g_pMGTriggerObject[m_mgTrigger].m_point)) {
                    // A soldier is on it.
                    m_mgTrigger = -1;
                    BSObjectTriggerEvent(m_pScriptObject, 0xA8, NULL, NULL, true);
                    return;
                }
                BSObject* point = g_pMGTriggerObject[m_mgTrigger].m_point->m_object;
                if (point != NULL) {
                    BSObjectTriggerEvent(point, 0xDA, (void*)1, NULL, true);
                }
                MarkMGAsUsed(g_pMGTriggerObject[m_mgTrigger].m_point, true);
                return;
            }
            m_mgTrigger = -1;
        }
    } else if (!on && (m_onMG || m_mounting)) {
        if (m_mgTrigger >= 0) {
            MarkMGAsUsed(g_pMGTriggerObject[m_mgTrigger].m_point, false);
            BSObject* point = g_pMGTriggerObject[m_mgTrigger].m_point->m_object;
            if (point != NULL) {
                BSObjectTriggerEvent(point, 0xDA, NULL, NULL, true);
            }
            m_mgTrigger = -1;
        }
        m_mgEjects = 0;
        m_pMGWeapon->Shoot((EWeaponShootType)1, 0.0f, -1.0f);
        if (m_curWeapon >= 0) {
            m_weapons[m_curWeapon]->m_active = 1;
        }
        if (!m_mgHier) {
            if (!m_mounting) {
                CVector3 pos(*(CVector3*)m_mgStartTM.m[3]);
                PlaceAfterMG(this, pos);
            }
            m_pMG->SetCollisionId((EClsnId)m_mgCollisionId);
            // The gun goes back to how it was placed.
            TriggerObjectData_struct* data = m_pMG->GetScriptObject()->m_trigger->m_data;
            CMatrix tm;
            ((CQuaternion*)data->m_orientation)->GetMatrix(tm);
            tm.SetPos(*(CVector3*)data->m_position);
            m_pMG->SetTMLocalToWorld(tm);
        } else {
            if (!m_mounting) {
                CVector3 pos;
                m_pMGHier->GetPosition(pos);
                CVector3 offset(*(CVector3*)m_mgStartTM.m[3]);
                pos.x = pos.x - offset.x;
                pos.y = pos.y - offset.y;
                pos.z = pos.z - offset.z;
                PlaceAfterMG(this, pos);
            }
            m_pMGHier->SetCollisionId((EClsnId)m_mgCollisionId);
            m_pMGHier->m_noUpdate = 0;
            CHierObject* part = m_pMG->AsHierObject();
            part->m_isMountedGun = 0;
            CQuaternion rot;
            rot.Set(0.0f, 0.0f, 0.0f, 1.0f);
            part->SetRotation(rot);
        }
        m_pMGWeapon->m_pProperties->m_refireRate = m_mgRefireRate;
        m_collisionId = 1;
        m_mgYaw = 0.0f;
        m_mgPitch = 0.0f;
        m_onMG = 0;
        m_mounting = 0;
        m_onMG = 0;
        m_mounting = 0;
        if (m_pMGWeapon == m_pMGCannon) {
            BSObjectTriggerEvent(m_pScriptObject, 0x17, NULL, NULL, true);
        }
    }
}

// A screen-space quad of an overlay sprite (u, v, x, y per corner).
static inline void SetQuadX(_spritepolyvert* v, float x0, float x1) {
    v[0].x = x0;
    v[1].x = x1;
    v[2].x = x1;
    v[3].x = x0;
}

// The HUD of this player's view: pop-up messages, multiplayer frag count,
// the crosshair, scope overlays (sniper scopes with their crosshair lines,
// binoculars) and the mounted gun's muzzle flash.
void CPlayerObject::Draw(CDrawContext& ctx) {
    if (ctx.field_0x114 != (int)field_0x768) {
        return;
    }
    if (!g_Pause.bIsGamePaused() && !g_bMPEndLevel) {
        g_pPopUpMessageHandler[field_0x768]->PopUpMessageHandler_Loop();
    }
    if (g_bInMultiplayerMode && m_dead && !g_bMPEndLevel && !g_Pause.bIsGamePaused()) {
        m_ui.DrawFragCount(false);
    }
    CWeapon* weapon = HeldWeapon(this);
    CPlayerWeaponObject* current;
    if (m_onMG) {
        current = NULL;
    } else if (m_curWeapon >= 0) {
        current = m_weapons[m_curWeapon];
    } else {
        current = NULL;
    }
    bool scope = false;
    m_scopeView = 0;
    if (m_zoomed && weapon != NULL && current != NULL && (!weapon->m_bReloading || weapon->m_reserveAmmo == 0)) {
        int mode = g_cheats.field_0x2 ? 3 : current->m_hideMode;
        m_scopeView = mode == 3;
        scope = current->m_pSprite0 != NULL;
    }
    if (!m_scopeView || !scope) {
        CSprite* crosshair = NULL;
        if (current != NULL) {
            crosshair = current->m_pSprite2;
        } else if (m_onMG) {
            crosshair = NULL;
        }
        bool on;
        if (g_bInMultiplayerMode) {
            on = g_Shell.m_players[m_controllerPort].m_crosshair != 0;
        } else {
            on = (u8)g_Shell.GetCrosshairStatus() != 0;
        }
        if (crosshair != NULL && !m_dead && !weapon->m_bReloading && !m_hideWeapon && (m_zoomed || on)) {
            int w = g_pScreen->GetWidth();
            int h = g_pScreen->GetHeight();
            int x = (float)w * ((g_viewport.m_left + g_viewport.m_right) * 0.5f);
            int y = (float)h * ((g_viewport.m_top + g_viewport.m_bottom) * 0.5f);
            crosshair->Draw((float)(x - 0x10), (float)(y - 8), CColor(0x80, 0x80, 0x80, 0x80));
        }
    }
    if (scope) {
        int w = g_pScreen->GetWidth();
        int h = g_pScreen->GetHeight();
        int type = weapon->m_weaponType;
        int left = (float)w * g_viewport.m_left;
        int top = (float)h * g_viewport.m_top;
        int right = (float)w * g_viewport.m_right;
        int bottom = (float)h * g_viewport.m_bottom;
        if (type != 0x1E && (type >= 0x1E || type >= 0x11 || type < 0xF)) {
            // Plain overlay over the view (split screen: square, with the
            // sides filled from the texture's corner).
            if (g_NumPlayers == 2) {
                int side = (right - left) / 4;
                _spritepolyvert v[4] = {
                    {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
                    {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
                    {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
                    {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
                };
                int inner = left + side;
                v[0].x = left;
                v[0].y = top;
                v[1].x = inner;
                v[1].y = top;
                v[2].x = inner;
                v[2].y = bottom;
                v[3].x = left;
                v[3].y = bottom;
                current->m_pSprite0->DrawPoly(v, 4, (CSprite::EPolyPrimType)5);
                int innerRight = right - side;
                SetQuadX(v, innerRight, right);
                current->m_pSprite0->DrawPoly(v, 4, (CSprite::EPolyPrimType)5);
                SetQuadX(v, inner, innerRight);
                v[1].u = 1.0f;
                v[2].u = 1.0f;
                v[2].v = 1.0f;
                v[3].v = 1.0f;
                current->m_pSprite0->DrawPoly(v, 4, (CSprite::EPolyPrimType)5);
            } else {
                _spritepolyvert v[4] = {
                    {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
                    {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
                    {1.0f, 1.0f, 0.0f, 0.0f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
                    {0.0f, 1.0f, 0.0f, 0.0f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
                };
                v[0].x = left;
                v[0].y = top;
                v[1].x = right;
                v[1].y = top;
                v[2].x = right;
                v[2].y = bottom;
                v[3].x = left;
                v[3].y = bottom;
                current->m_pSprite0->DrawPoly(v, 4, (CSprite::EPolyPrimType)5);
            }
        } else {
            // A sniper scope: black side bars, the scope texture mirrored
            // into the four quarters, then the reticle.
            int bar;
            if (g_NumPlayers == 2) {
                bar = (33.0f * g_viewport.m_width) + (float)((right - left) / 4);
            } else {
                bar = 66.0f * g_viewport.m_width;
            }
            _spritepolyvert v[4] = {
                {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
                {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
                {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
                {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
            };
            int inner = left + bar;
            v[0].x = left;
            v[0].y = top;
            v[1].x = inner;
            v[1].y = top;
            v[2].x = inner;
            v[2].y = bottom;
            v[3].x = left;
            v[3].y = bottom;
            current->m_pSprite0->DrawPoly(v, 4, (CSprite::EPolyPrimType)5);
            int innerRight = right - bar;
            SetQuadX(v, innerRight, right);
            current->m_pSprite0->DrawPoly(v, 4, (CSprite::EPolyPrimType)5);
            int width = right - left;
            int cx = left + (width >> 1);
            int cy = top + ((bottom - top) >> 1);
            // Top left.
            SetQuadX(v, inner, cx);
            v[2].y = cy;
            v[3].y = cy;
            v[1].u = 1.0f;
            v[2].u = 1.0f;
            v[2].v = 1.0f;
            v[3].v = 1.0f;
            current->m_pSprite0->DrawPoly(v, 4, (CSprite::EPolyPrimType)5);
            // Bottom left.
            v[0].y = cy;
            v[1].y = cy;
            v[2].y = bottom;
            v[3].y = bottom;
            v[0].u = 1.0f;
            v[1].u = 1.0f;
            v[1].v = 1.0f;
            v[2].u = 0.0f;
            v[3].v = 0.0f;
            current->m_pSprite0->DrawPoly(v, 4, (CSprite::EPolyPrimType)5);
            // Bottom right.
            SetQuadX(v, cx, innerRight);
            v[0].v = 1.0f;
            v[1].v = 0.0f;
            v[2].v = 0.0f;
            v[3].v = 1.0f;
            current->m_pSprite0->DrawPoly(v, 4, (CSprite::EPolyPrimType)5);
            // Top right.
            v[1].u = 0.0f;
            v[0].y = top;
            v[1].y = top;
            v[2].y = cy;
            v[3].y = cy;
            v[0].v = 0.0f;
            v[1].u = 0.0f;
            v[2].v = 1.0f;
            v[3].u = 1.0f;
            current->m_pSprite0->DrawPoly(v, 4, (CSprite::EPolyPrimType)5);
            if (weapon->m_weaponType == 0xF) {
                // Reticle: a vertical and a horizontal line.
                v[1].x = cx + 1;
                v[2].x = cx + 1;
                v[2].y = bottom;
                v[3].y = bottom;
                v[0].u = 0.0f;
                v[0].v = 0.0f;
                v[2].v = 0.0f;
                v[3].u = 0.0f;
                v[3].v = 0.0f;
                current->m_pSprite0->DrawPoly(v, 4, (CSprite::EPolyPrimType)5);
                v[0].x = left;
                v[0].y = cy;
                v[1].x = right;
                v[1].y = cy;
                v[2].x = right;
                v[2].y = cy + 1;
                v[3].x = left;
                v[3].y = cy + 1;
                current->m_pSprite0->DrawPoly(v, 4, (CSprite::EPolyPrimType)5);
            } else if (weapon->m_weaponType == 0x1E) {
                // The second overlay inside the scope.
                int inset;
                if (g_NumPlayers == 2) {
                    inset = (54.0f * g_viewport.m_width) + (float)(width / 4);
                } else {
                    inset = 108.0f * g_viewport.m_width;
                }
                _spritepolyvert r[4] = {
                    {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
                    {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
                    {1.0f, 1.0f, 0.0f, 0.0f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
                    {0.0f, 1.0f, 0.0f, 0.0f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
                };
                int x0 = left + inset;
                int y0 = top + (int)(82.0f * g_viewport.m_height);
                int x1 = right - inset;
                int y1 = bottom - (int)(68.0f * g_viewport.m_height);
                r[0].x = x0;
                r[0].y = y0;
                r[1].x = x1;
                r[1].y = y0;
                r[2].x = x1;
                r[2].y = y1;
                r[3].x = x0;
                r[3].y = y1;
                current->m_pSprite1->DrawPoly(r, 4, (CSprite::EPolyPrimType)5);
            } else {
                // Reticle: a vertical line below the centre.
                v[0].y = bottom;
                v[1].x = cx - 1;
                v[1].y = bottom;
                v[2].x = cx - 1;
                v[0].u = 0.0f;
                v[0].v = 0.0f;
                v[2].v = 0.0f;
                v[3].u = 0.0f;
                v[3].v = 0.0f;
                current->m_pSprite0->DrawPoly(v, 4, (CSprite::EPolyPrimType)5);
                v[1].x = cx + 1;
                v[2].x = cx + 1;
                current->m_pSprite0->DrawPoly(v, 4, (CSprite::EPolyPrimType)5);
            }
        }
    }
    if (m_onMG && m_pMGWeapon != m_pMGCannon && (u8)m_pMGWeapon->GetMuzzleFlashStatus() && !g_cheats.field_0xB &&
        g_pStaticObjectMuzzleFlashPlayer != NULL) {
        g_pStaticObjectMuzzleFlashPlayer->Draw(ctx);
    }
}

// Controller inputs (CInputManager::GetValue).
enum {
    INPUT_MOVE_Y = 7,
    INPUT_STRAFE = 8,
    INPUT_TURN = 9,
    INPUT_LOOK = 0xA,
    INPUT_JUMP = 0xB,
    INPUT_CROUCH = 0xC,
    INPUT_USE = 0xD,
    INPUT_FIRE = 0xE,
    INPUT_EVENT_11 = 0xF,
    INPUT_ZOOM = 0x10,
    INPUT_EVENT_12 = 0x13,
    INPUT_EVENT_13 = 0x14,
    INPUT_EVENT_14 = 0x15,
    INPUT_CENTER = 0x16,
    INPUT_HINT = 0x7E,
    INPUT_LOOK_ALT = 0x8A,  // control scheme 2
    INPUT_ZOOM_OUT = 0x8E,
    INPUT_ZOOM_IN = 0x8F,
};

// Look input scale (radians per unit per frame).
#define LOOK_RATE 0.0003515625f

static inline float InputValue(CPlayerObject* player, int input) {
    return (float)g_inputMgr.GetValue(player->m_controllerPort, input);
}

// -1 with "invert look" on.
static inline float LookInvert(CPlayerObject* player) {
    if (g_bInMultiplayerMode) {
        return g_Shell.m_players[player->m_controllerPort].m_invertLook ? -1.0f : 1.0f;
    }
    return g_inputMgr.field_0x244A ? -1.0f : 1.0f;
}

// The scheme-2 controller layout (multiplayer: per player).
static inline bool AltControls(CPlayerObject* player) {
    if (g_bInMultiplayerMode) {
        return g_Shell.PassPlayersMultiPlayerControllerValue(player->m_controllerPort) == 2;
    }
    return g_Shell.Get_currentControllerSetting() == 2;
}

// Mounted gun shaking while it fires.
static float osc = 0.0f;
static float osc_add = 1.0f;
static float max_osc = 1.0f;
static float osc_dist = 0.006f;

// The player's frame: weapons' light volumes, input (look, move, jump,
// crouch, lean, zoom, fire, use), the mounted gun, movement with gravity
// and falling damage, explosion pushes, recoil, the eye matrix, script
// events and sounds.
int CPlayerObject::BeginUpdate(float dt) {
    for (int i = 0; i < NUM_WEAPONS; i++) {
        if (m_weapons[i] != NULL) {
            m_weapons[i]->m_lightVolumes.Update(dt);
        }
    }
    bool grounded = false;
    float lookPitch = 0.0f;
    float lookYaw = 0.0f;
    if (!m_climbing) {
        // Ground just under the feet?
        const IVolume* world = g_scene.m_world->GetWorldBoundingVolume((ISceneNode::EVolumeType)0);
        CVector3 pos(*(CVector3*)m_tm.m[3]);
        CLine3 line(pos, CVector3(pos.x, pos.y, (pos.z - m_localCapsule.m_radius) - 0.05f));
        CVolFiber fiber(line);
        CCollision clsn;
        CCDBObject::DisablePassThru(true);
        int result = world->TestCollision(fiber, clsn, false);
        CCDBObject::DisablePassThru(false);
        bool hit = false;
        if (result != CLSN_NONE || m_onGround) {
            hit = true;
        }
        grounded = hit;
    }
    if (g_inputMgr.GetValue(m_controllerPort, INPUT_CENTER) != 0) {
        if (m_onMG) {
            m_mgPitch = 0.0f;
        } else if (m_attachedKeepWeapon) {
            m_pitch = 0.0f;
        }
    }
    bool jump = g_inputMgr.GetValue(m_controllerPort, INPUT_JUMP) != 0;
    if (jump && m_onGround && !m_jumping) {
        BSObjectTriggerEvent(m_pScriptObject, 0x114, NULL, NULL, false);
        m_jumping = 1;
    }
    if (g_cheats.field_0x3) {
        // Slow motion for a while after each jump.
        if (g_cheats.field_0x4 > 0.0f) {
            g_cheats.field_0x4 = g_cheats.field_0x4 - dt;
        }
        if (jump) {
            g_cheats.field_0x4 = 45.0f;
        } else if (g_cheats.field_0x4 <= 0.0f) {
            g_cheats.field_0x4 = 0.0f;
        }
    }
    if (fabs(m_fovRate) > 0.0) {
        m_fov = 0.016683351f * (m_fovRate * dt) + m_fov;
        if (m_fovRate > 0.0f) {
            if (m_fov >= m_fovTarget) {
                m_fovRate = 0.0f;
                m_fov = m_fovTarget;
            }
        } else if (m_fov <= m_fovTarget) {
            m_fovRate = 0.0f;
            m_fov = m_fovTarget;
        }
    }
    if (g_bInMultiplayerMode && m_fireDelay > 0) {
        m_fireDelay -= (int)dt;
    }

    // Zoom: the scope's own zoom (D-pad), binoculars, or none.
    CWeapon* weapon = GetCurrentWeapon();
    if (weapon != NULL && weapon->m_bReloading && weapon->m_reserveAmmo != 0) {
        m_scopeView = 0;
    }
    g_inputMgr.m_dpadLocked[m_controllerPort] = m_scopeView;
    if (m_scopeView) {
        float zoom = InputValue(this, INPUT_ZOOM_IN);
        zoom = zoom - InputValue(this, INPUT_ZOOM_OUT);
        if (fabs(zoom) > 0.0) {
            m_scopeFov = 59.94f * (0.01f * zoom * dt) + m_scopeFov;
            m_scopeFov = MathFunClamp<float>(m_scopeFov, 5.0f, 35.0f);
        }
        TransitionFOV(m_scopeFov, 140.0f);
    } else if (weapon != NULL && weapon->m_bReloading) {
        TransitionFOV(35.0f, 140.0f);
    } else if (m_zoomed) {
        CPlayerWeaponObject* current = GetCurrentPlayerWeapon();
        if (current != NULL && (g_cheats.field_0x2 ? 3 : current->m_hideMode) == 2) {
            TransitionFOV(current->field_0x3184, 140.0f);
        } else {
            TransitionFOV(35.0f, 140.0f);
        }
    } else {
        TransitionFOV(35.0f, 140.0f);
    }
    m_wasZoomed = m_zoomed;
    int zoomInput = g_inputMgr.GetValue(m_controllerPort, INPUT_ZOOM);
    if (GetCurrentPlayerWeapon() != NULL && (g_cheats.field_0x2 ? 3 : GetCurrentPlayerWeapon()->m_hideMode) != 0 &&
        !m_wielding) {
        m_zoomed = zoomInput > 0;
    } else {
        m_zoomed = 0;
    }
    if (!m_zoomed) {
        m_peeking = 0;
    }
    if (m_onMG || m_mounting || !m_attachedKeepWeapon) {
        m_zoomed = 0;
    }
    if (!m_wasZoomed && m_zoomed) {
        // Leaning is measured from here.
        m_peekOrigin = *(CVector3*)m_tm.m[3];
        field_0x78 = 0.0f;
        field_0x7C = 0.0f;
    }

    // Crouching (and, zoomed, leaning up or down with the stick).
    if (m_crouched && !m_canCrouch) {
        BeginStand();
    } else if (g_inputMgr.GetValue(m_controllerPort, INPUT_CROUCH) != 0 && m_attachedKeepWeapon && grounded) {
        if (m_crouched) {
            BeginStand();
        } else {
            BeginCrouch();
        }
    }
    if (!m_crouchMoving && m_attachedKeepWeapon) {
        if (m_crouched) {
            float amount = 0.0f;
            if (m_zoomed && !AltControls(this)) {
                amount = 0.0078125f * (float)-g_inputMgr.GetValue(m_controllerPort, INPUT_MOVE_Y);
                if (amount < 0.01f) {
                    amount = 0.0f;
                }
            }
            PeekUp(amount);
        } else if (m_canCrouch) {
            float amount = 0.0f;
            if (m_zoomed && !AltControls(this) && grounded) {
                amount = 0.0078125f * InputValue(this, INPUT_MOVE_Y);
                if (amount < 0.01f) {
                    amount = 0.0f;
                }
            }
            DuckDown(amount);
        }
        float maxRate = g_bInMultiplayerMode ? 0.02666667f : 0.07333334f;
        if (m_eyeHeightRate > maxRate) {
            m_eyeHeightRate = maxRate;
        } else if (m_eyeHeightRate < -maxRate) {
            m_eyeHeightRate = -maxRate;
        }
        m_eyeHeightRate = m_eyeHeightRate / dt;
    }
    g_CameraOffset.z = 0.1f + m_localCapsule.m_line.m_end.z;

    if (m_zoomed) {
        // Aiming: slower look (scaled by the scope's zoom), leaning only.
        if (m_attachedKeepWeapon) {
            float invert = LookInvert(this);
            if (AltControls(this)) {
                lookPitch = LOOK_RATE * (invert * InputValue(this, INPUT_LOOK_ALT));
            } else {
                lookPitch = LOOK_RATE * (invert * InputValue(this, INPUT_LOOK));
            }
            lookYaw = LOOK_RATE * (float)-g_inputMgr.GetValue(m_controllerPort, INPUT_TURN);
            CPlayerWeaponObject* current = GetCurrentPlayerWeapon();
            if (current != NULL) {
                float sens = 1.0f;
                if (m_scopeView) {
                    sens = 0.26000002f * ((m_fov - 5.0f) / 30.0f) + 0.04f;
                } else if ((g_cheats.field_0x2 ? 3 : current->m_hideMode) == 2) {
                    sens = current->field_0x3188;
                }
                if (m_superAim) {
                    sens *= 2.0f;
                }
                lookPitch = lookPitch * sens;
                lookYaw = lookYaw * sens;
            }
        } else {
            lookPitch = 0.0f;
            lookYaw = 0.0f;
        }
        if (!m_flying) {
            m_localVelocity.x = 0.0f;
            m_localVelocity.y = 0.0f;
            if (!m_attached) {
                PeekLeftRight();
            }
        } else {
            m_peekOrigin = *(CVector3*)m_tm.m[3];
        }
        if (m_onGround) {
            m_flying = 0;
            m_localVelocity.z = 0.0f;
        }
        jump = false;
    } else if (m_mounting) {
        // Walking up to a mounted gun: get on if within its arc.
        CVector3 gunPos;
        m_pMG->GetPosition(gunPos);
        CMatrix pivot;
        m_pMG->GetTMLocalToWorld(pivot);
        pivot.Multiply(m_mgPivotTM, pivot);
        CVector3 front;
        front = *(CVector3*)pivot.m[1];
        CVector3 dir;
        dir = *(CVector3*)m_tm.m[3];
        dir.Sub(gunPos, dir);  // toward the gun
        dir.z = 0.0f;
        dir.Normalize();
        float d = dir.z * front.z + (dir.x * front.x + dir.y * front.y);
        if (d < (float)cos(m_mgYawLimit) && m_mgYawLimit < 3.1415927f) {
            BSObjectTriggerEvent(m_pScriptObject, 0xA8, NULL, NULL, true);
        } else {
            m_onMG = 1;
            m_mounting = 0;
            if (m_curWeapon >= 0) {
                m_weapons[m_curWeapon]->m_active = 0;
            }
            m_mgYaw = 0.0f;
            m_mgPitch = 0.0f;
            m_localVelocity.z = 0.0f;
            m_localVelocity.y = 0.0f;
            m_localVelocity.x = 0.0f;
            m_pitch = 0.0f;
            m_tm = pivot;
            g_CameraOffset.x = 0.0f;
            g_CameraOffset.y = 0.0f;
            g_CameraOffset.z = 0.0f;
        }
    } else if (m_onMG) {
        if (m_mgHier) {
            // A turret: its own turning changes the aim's yaw.
            CHierObject* part = m_pMG->AsHierObject();
            m_pMGHier->DoBeginUpdate(dt, true);
            CMatrix parent;
            parent = part->m_parentTM;
            CMatrix copy;
            copy = parent;
            CVector3 front(*(CVector3*)copy.m[1]);
            float was = MathFunAtan2F(m_mgPivot.x, m_mgPivot.y);
            float now = MathFunAtan2F(front.x, front.y);
            m_mgPivot = front;
            m_mgYaw = m_mgYaw - (now - was);
            if (m_mgYaw > 3.1415927f) {
                m_mgYaw = m_mgYaw - 6.2831855f;
            }
            if (m_mgYaw < -3.1415927f) {
                m_mgYaw = m_mgYaw + 6.2831855f;
            }
        }
        float turn = m_pMGWeapon == m_pMGCannon ? 0.045f : MMG_TURNSPEED;
        float invert = g_inputMgr.field_0x244A ? -1.0f : 1.0f;
        int lookInput = g_Shell.Get_currentControllerSetting() == 2 ? INPUT_LOOK_ALT : INPUT_LOOK;
        lookPitch = m_mgCannonScale * ((0.0078125f * turn) * (invert * InputValue(this, lookInput)));
        lookYaw = m_mgCannonScale * ((0.0078125f * turn) * (float)-g_inputMgr.GetValue(m_controllerPort, INPUT_TURN));
        if (m_pMGWeapon == m_pMGCannon) {
            // The turret's turning sound.
            if (lookPitch == 0.0f && lookYaw == 0.0f) {
                if (m_mgTurning) {
                    BSObjectTriggerEvent(m_pScriptObject, 0x17, NULL, NULL, true);
                    m_mgTurning = 0;
                }
            } else if (!m_mgTurning) {
                BSObjectTriggerEvent(m_pScriptObject, 0x16, NULL, NULL, true);
                m_mgTurning = 1;
            }
        }
        ShakeCamera(lookPitch, lookYaw, dt);
        m_mgYaw = lookYaw * dt + m_mgYaw;
        m_mgPitch = lookPitch * dt + m_mgPitch;
        if (m_mgYawLimit < 3.1415927f) {
            m_mgYaw = m_mgYaw >= m_mgYawLimit ? m_mgYawLimit : m_mgYaw;
            m_mgYaw = m_mgYaw <= -m_mgYawLimit ? -m_mgYawLimit : m_mgYaw;
        }
        m_mgPitch = m_mgPitch >= m_mgPitchLimit ? m_mgPitchLimit : m_mgPitch;
        m_mgPitch = m_mgPitch <= -m_mgPitchLimit ? -m_mgPitchLimit : m_mgPitch;
        if (m_mgYaw > 6.2831855f) {
            m_mgYaw = m_mgYaw - 6.2831855f;
        } else if (m_mgYaw < -6.2831855f) {
            m_mgYaw = m_mgYaw + 6.2831855f;
        }
        CMatrix gun;
        if (!m_mgHier) {
            // Turn the gun from its placed orientation about the pivot.
            TriggerObjectData_struct* data = m_pMG->GetScriptObject()->m_trigger->m_data;
            ((CQuaternion*)data->m_orientation)->GetMatrix(gun);
            gun.RotateZ(-m_mgYaw);
            CMatrix pivot;
            pivot = gun;
            pivot.Multiply(m_mgPivotTM, pivot);
            gun.Rotate(*(CVector3*)pivot.m[0], -m_mgPitch);
            gun.SetPos(CVector3(data->m_position[0], data->m_position[1], data->m_position[2]));
            m_pMG->SetTMLocalToWorld(gun);
            SetPlayersMountedMGPositions(&gun);
        } else {
            CHierObject* part = m_pMG->AsHierObject();
            gun.Ident();
            gun.RotateZ(-m_mgYaw);
            CMatrix pivot;
            pivot = gun;
            pivot.Multiply(m_mgPivotTM, pivot);
            gun.Rotate(*(CVector3*)pivot.m[0], -m_mgPitch);
            CQuaternion rot;
            rot.SetFromMatrix(gun);
            part->SetRotation(rot);
            part->DoBeginUpdate(dt, false);
            m_pMG->GetTMLocalToWorld(gun);
            SetPlayersMountedMGPositions(&gun);
        }
        m_localVelocity.z = 0.0f;
        m_localVelocity.y = 0.0f;
        m_localVelocity.x = 0.0f;
        g_CameraOffset.x = 0.0f;
        g_CameraOffset.y = 0.0f;
        g_CameraOffset.z = 0.0f;
        if (m_pMGWeapon != m_pMGCannon) {
            // The muzzle flash object, randomly spun.
            float spin = MathFunRandomReal(0.0f, 6.2831855f);
            if (g_pStaticObjectMuzzleFlashPlayer != NULL) {
                CMatrix flash;
                flash.BuildScale(m_mgFlashScale);
                flash.RotateY(spin);
                flash.Multiply(flash, m_mgMuzzleTM);
                CMatrix tm;
                m_pMG->GetTMLocalToWorld(tm);
                flash.Multiply(flash, tm);
                g_pStaticObjectMuzzleFlashPlayer->SetTMLocalToWorld(flash);
            }
        }
    } else {
        // On foot.
        field_0x44C = 0.0f;
        field_0x450 = 0.0f;
        if (grounded) {
            m_flying = 0;
        }
        if (!m_attached) {
            float strafe = 0.0078125f * InputValue(this, INPUT_STRAFE);
            float forward = 0.0078125f * (float)-g_inputMgr.GetValue(m_controllerPort, INPUT_MOVE_Y);
            if (m_climbing) {
                m_localVelocity.x = 1.1f * (0.09175843f * strafe);
                m_localVelocity.y = m_localVelocity.y * 0.5f;
                m_localVelocity.z = m_pitch < 0.0f ? forward : -forward;
                m_localVelocity.z = m_localVelocity.z * (m_localVelocity.z > 0.0f ? 0.04170838f : 0.025025027f);
            } else if (!m_flying) {
                if (grounded) {
                    m_localVelocity.x = 0.09175843f * strafe;
                    m_localVelocity.y = forward;
                    if (g_bInMultiplayerMode) {
                        m_localVelocity.y = m_localVelocity.y * 0.09175843f;
                    } else {
                        // Backing up is slower.
                        m_localVelocity.y = m_localVelocity.y * (m_localVelocity.y > 0.0f ? 0.09175843f : 0.066733405f);
                    }
                    m_slopeGrip = 1.0f;
                } else {
                    // Some air control.
                    m_localVelocity.x = 0.09175843f * (1.1f * strafe) + m_localVelocity.x;
                    m_localVelocity.y = 0.09175843f * (1.1f * forward) + m_localVelocity.y;
                    float speedSq = m_localVelocity.x * m_localVelocity.x + m_localVelocity.y * m_localVelocity.y;
                    if (speedSq > 0.00841961f) {
                        float k = 0.09175843f / sqrtf(speedSq);
                        m_localVelocity.x = m_localVelocity.x * k;
                        m_localVelocity.y = m_localVelocity.y * k;
                    }
                }
                if (m_onGround) {
                    m_localVelocity.z = 0.0f;
                }
                if (m_onSlope) {
                    // Steep slopes: the grip goes and the player slows.
                    if (m_slopeGrip > 0.01f) {
                        m_slopeGrip = m_slopeGrip * (float)pow(0.9900000095367432, dt);
                    }
                    if (m_slopeGrip < 0.01f) {
                        m_slopeGrip = 0.01f;
                    }
                    m_localVelocity.x = m_localVelocity.x * m_slopeGrip;
                    m_localVelocity.y = m_localVelocity.y * m_slopeGrip;
                }
            }
        } else {
            m_localVelocity.z = 0.0f;
            m_localVelocity.y = 0.0f;
            m_localVelocity.x = 0.0f;
        }
        if (jump && m_climbing) {
            m_climbing = 0;
        }
        if (m_crouched && grounded) {
            m_localVelocity.x = m_localVelocity.x * 0.25f;
            m_localVelocity.y = m_localVelocity.y * 0.25f;
        }
        if (m_attachedKeepWeapon) {
            lookPitch = LOOK_RATE * (LookInvert(this) * InputValue(this, INPUT_LOOK));
            lookYaw = LOOK_RATE * (float)-g_inputMgr.GetValue(m_controllerPort, INPUT_TURN);
        } else {
            lookPitch = 0.0f;
            lookYaw = 0.0f;
        }
    }

    // Gravity; falling damage on landing.
    if (!m_attached && !m_onGround && !m_mounting && !m_onMG && !m_climbing) {
        if (m_gravity) {
            m_localVelocity.z = -0.005455351f * dt + m_localVelocity.z;
        }
        if (!m_falling && m_localVelocity.z <= 0.0f) {
            m_fallStart = m_tm.m[3][2];
            m_falling = 1;
        }
    }
    float fall = 0.0f;
    if (m_falling && (grounded || m_onSlope)) {
        m_falling = 0;
        fall = m_fallStart - m_tm.m[3][2];
        if (fall > 4.0f) {
            if (g_cheats.m_invincible) {
                TriggerScriptEvent(0xE8, NULL, false);
            } else if (m_fallingDamage) {
                m_ui.AddHitDirection(1);
                m_ui.AddHitDirection(3);
                m_ui.AddHitDirection(4);
                m_ui.AddHitDirection(2);
                g_inputMgr.SetFeedback(m_controllerPort, 0xC, 0, 1.0f, 1.1f);
                TriggerScriptEvent(0xE8, (void*)((int)(fall - 4.0f) * 5), false);
            }
        }
    }
    if (m_pushed && !m_attached) {
        // An explosion: thrown, without control until landing.
        m_pushed = 0;
        m_flying = 1;
        CMatrix inv;
        inv.FastInverse(m_tm);
        CVector3 local;
        inv.TransformVector(local, field_0x278);
        m_localVelocity.Add(local);
        if (m_localVelocity.z < 0.0f) {
            m_localVelocity.z = MathFunClamp<float>(m_localVelocity.z, -0.21f, 0.0f);
        } else {
            m_localVelocity.z = MathFunClamp<float>(m_localVelocity.z, 0.0f, 0.21f);
        }
    }
    if (grounded && jump && !m_attached && !m_crouched && !m_crouchMoving) {
        m_localVelocity.z = m_localVelocity.z + 0.09175843f;
        if (m_flying) {
            if (m_localVelocity.z < 0.0f) {
                m_localVelocity.z = MathFunClamp<float>(m_localVelocity.z, -0.21f, 0.0f);
            } else {
                m_localVelocity.z = MathFunClamp<float>(m_localVelocity.z, 0.0f, 0.21f);
            }
        }
    } else {
        jump = false;
    }
    if (m_crouchMoving && !m_attached && m_eyeHeightRate > 0.0f) {
        m_localVelocity.y = 0.0f;
        m_localVelocity.x = 0.0f;
    }
    m_tm.TransformVector(m_velocity, m_localVelocity);
    if (m_deathTime > 0.0f || m_dead) {
        m_velocity.x = 0.0f;
        m_velocity.y = 0.0f;
        m_localVelocity.x = 0.0f;
        m_localVelocity.y = 0.0f;
        if (m_deathTime > 0.0f) {
            UpdateDeathSequence(dt);
        }
    }

    // Looking, with recoil and its recovery.
    if (!m_onMG) {
        ShakeCamera(lookPitch, lookYaw, dt);
        m_pitch = lookPitch * dt + m_pitch;
        m_yaw = lookYaw * dt + m_yaw;
        if (m_yaw > 6.2831854820251465) {
            m_yaw = m_yaw - 6.2831854820251465;
        } else if (m_yaw < -6.2831854820251465) {
            m_yaw = m_yaw + 6.2831854820251465;
        }
        if (m_recoilOffset < 0.0f) {
            if (m_pitch > m_lastPitch) {
                m_recoilOffset = m_recoilOffset + (m_pitch - m_lastPitch);
            }
            float back;
            if (m_recoilOffset < 0.0f) {
                float step = 0.003f * GetCurrentPlayerWeapon()->field_0x3194 * dt;
                if (-step < m_recoilOffset) {
                    back = -m_recoilOffset;
                    m_recoilOffset = 0.0f;
                } else {
                    back = step;
                    m_recoilOffset = m_recoilOffset + step;
                }
            } else {
                back = 0.0f;
                m_recoilOffset = 0.0f;
            }
            m_pitch = m_pitch + back;
        }
        m_pitch = MathFunClamp<float>(m_pitch, -1.45f, 1.45f);
        float before = m_pitch;
        float kick = MathFunClamp<float>(m_recoilPitch * dt, 0.0f, m_recoilPitch);
        m_pitch = m_pitch - kick;
        if (fabs(m_recoilYaw) > 0.0) {
            m_yaw = m_yaw + MathFunRandomRealSigned(-m_recoilYaw * 0.5f, m_recoilYaw * 0.5f);
            m_recoilYaw = 0.0f;
        }
        m_recoilKick = -kick;
        m_pitch = MathFunClamp<float>(m_pitch, -1.45f, 1.45f);
        if (fabs(m_recoilPitch) < 0.0010000000474974513) {
            m_recoilPitch = 0.0f;
        }
        m_recoilOffset = m_recoilOffset + (m_pitch - before);
        m_lastPitch = m_pitch;
        m_recoilPitch = m_recoilPitch - kick;
        if (m_recoilPitch < 0.0f) {
            m_recoilPitch = 0.0f;
        }
    }
    UpdateMovePath(dt);
    if (!m_attached || m_attachedKeepWeapon) {
        m_eyeTransform.BuildRotX(-m_pitch);
        m_eyeTransform.Translate(g_CameraOffset);
    } else {
        m_eyeTransform.BuildTrans(g_CameraOffset);
    }
    CVector3 cameraPos;
    g_pCamera->GetPosition(cameraPos);
    m_camera.GetForward(m_viewDir);

    // Firing (and grenades: held to throw further, or cooked).
    float fire = InputValue(this, INPUT_FIRE);
    if (m_grenadeHoldTime >= 0.0f && !m_hideWeapon) {
        m_grenadeHoldTime = m_grenadeHoldTime + dt;
        if (m_grenadeHoldTime > 120.0) {
            m_grenadeHoldTime = 120.0f;
        }
    }
    bool fired = false;
    if (m_dead) {
        goto done;
    }
    if (m_cookTime >= 0.0 && !m_hideWeapon) {
        if (m_cookTime < dt) {
            // Cooked too long: it goes off in hand.
            FireMyWeapon(0.0f);
            BSObjectTriggerEvent(m_pScriptObject, 0xA0, NULL, NULL, true);
            m_cookTime = -1.0f;
            fired = true;
        } else {
            m_cookTime = m_cookTime - dt;
        }
    }
    if (!fired && !m_hideWeapon && (m_curWeapon >= 0 || m_onMG)) {
        if (fire != 0.0f) {
            if (m_fireDelay <= 0) {
                BSObjectTriggerEvent(m_pScriptObject, 0x10, *(void**)&fire, NULL, true);
                m_lastFire = fire;
                if (m_onMG && m_pMGWeapon != m_pMGCannon) {
                    if (!m_mgHier) {
                        CMatrix gun;
                        m_pMG->GetTMLocalToWorld(gun);
                        CVector3 pos;
                        pos.Scale(*(CVector3*)gun.m[1], osc_dist * osc);
                        pos.Add(*(CVector3*)gun.m[3]);
                        gun.SetPos(pos);
                        m_pMG->SetTMLocalToWorld(gun);
                    } else {
                        osc_dist = 0.02f;
                        CVector3 pos;
                        pos.Scale(*(CVector3*)m_mgPivotTM.m[1], osc_dist * osc);
                        pos.Add(*(CVector3*)m_mgPivotTM.m[3]);
                        m_mgPivotTM.SetPos(pos);
                    }
                    osc = osc_add * dt + osc;
                    if (osc >= max_osc || osc <= -max_osc) {
                        osc = osc_add * max_osc;
                        osc_add = -osc_add;
                    }
                }
            }
        } else if (m_lastFire != 0.0f) {
            // Released.
            BSObjectTriggerEvent(m_pScriptObject, 0x94, *(void**)&fire, NULL, true);
            if (m_onMG) {
                m_pMGWeapon->Shoot((EWeaponShootType)1, 0.0f, -1.0f);
            } else {
                m_weapons[m_curWeapon]->Shoot((EWeaponShootType)1, 0);
            }
        } else if (m_onMG) {
            m_pMGWeapon->Shoot((EWeaponShootType)1, 0.0f, -1.0f);
        } else {
            m_weapons[m_curWeapon]->Shoot((EWeaponShootType)1, 0);
        }
    }
    if (!m_hideWeapon) {
        if (g_inputMgr.GetValue(m_controllerPort, INPUT_EVENT_14) != 0) {
            BSObjectTriggerEvent(m_pScriptObject, 0x14, NULL, NULL, false);
        }
        if (g_inputMgr.GetValue(m_controllerPort, INPUT_EVENT_11) != 0) {
            BSObjectTriggerEvent(m_pScriptObject, 0x11, NULL, NULL, false);
        }
        if (g_inputMgr.GetValue(m_controllerPort, INPUT_EVENT_12) != 0) {
            BSObjectTriggerEvent(m_pScriptObject, 0x12, NULL, NULL, false);
        }
        if (g_inputMgr.GetValue(m_controllerPort, INPUT_EVENT_13) != 0) {
            BSObjectTriggerEvent(m_pScriptObject, 0x13, NULL, NULL, false);
        }
        if (g_inputMgr.GetValue(m_controllerPort, INPUT_USE) != 0) {
            if (!g_bInMultiplayerMode) {
                if (m_triggerChecks && m_grenadeHoldTime < 0.0f && m_cookTime < 0.0f) {
                    TriggerActivateMessages(m_pScriptObject->m_gameObject, false);
                }
            } else if (g_Shell.PassPlayersMultiPlayerControllerValue(m_controllerPort) != 2) {
                BSObjectTriggerEvent(m_pScriptObject, 0x12, NULL, NULL, false);
            }
        }
    }
    {
        // Script event 0x82 when the gait changes.
        float speed = m_velocity.Length();
        if (speed > 0.07340675f) {
            if (m_moveState != 2) {
                BSObjectTriggerEvent(m_pScriptObject, 0x82, (void*)2, NULL, false);
                m_moveState = 2;
            }
        } else if (speed > 0.009175843f) {
            if (m_moveState != 1) {
                BSObjectTriggerEvent(m_pScriptObject, 0x82, (void*)1, NULL, false);
                m_moveState = 1;
            }
        } else if (m_moveState != 0) {
            BSObjectTriggerEvent(m_pScriptObject, 0x82, (void*)0, NULL, false);
            m_moveState = 0;
        }
    }
done:
    field_0x288 = *(CVector3*)m_tm.m[3];
    m_eyeHeight = m_localCapsule.m_line.m_end.z;
    m_aiDoodad.PreUpdate();
    PlaySounds(grounded, jump, fall, dt);
    if (g_inputMgr.GetValue(m_controllerPort, INPUT_HINT) != 0 && !g_bInMultiplayerMode) {
        DisplayHint();
    }
}
