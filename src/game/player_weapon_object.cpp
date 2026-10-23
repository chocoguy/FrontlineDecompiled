// CPlayerWeaponObject: the player's first-person weapons (see
// player_weapon_object.h).
#include "game/player_weapon_object.h"
#include "game/player.h"
#include "game/weapon.h"
#include "game/weaponfactory.h"
#include "game/thrown_obj.h"
#include "game/objcreate.h"
#include "game/AnimObjFactory.h"
#include "game/mainloop.h"
#include "game/scene.h"
#include "game/camera.h"
#include "game/draw_context.h"
#include "game/particlesystem.h"
#include "game/sprite.h"
#include "game/Screen.h"
#include "game/viewport.h"
#include "game/gameconfig.h"
#include "game/ShellMenu.h"
#include "game/MathFun.h"
#include "game/Moh2.h"
#include "game/toilet.h"
#include "game/texpack.h"
#include "math.h"
#include "string.h"

// Shell casing types with their own ejection.
#define SHELL_PISTOL 0xF2C8FB78
#define SHELL_SHOTGUN 0xB2134D62
// The muzzle flash particles when the weapon names none.
#define PARTICLES_MUZZLE_FLASH 0x67D7FA88

// The projection the weapon is drawn with (the scene's, replaced in Draw).
static CMatrix g_Cam2clip;
// The weapon's near plane, as a fraction of the camera's.
static float g_fNearMultiplier = 0.05f;

int CPlayerWeaponObject::IsVisible(CDrawContext&) const {
    return IsDrawEnabled();
}

// Multiplayer: the arms' texture.
void CPlayerWeaponObject::SetSleeveTexture(char* name) {
    bool loaded = false;
    LevelFileContentsStruct_* res = TLT_FindNextResourceByType((TLTResourceID)0xD, NULL);
    CTexturePack* pack =
        (CTexturePack*)TLT_LoadFromRepository(ADDR_TO_PTR(const char, res->fields[28].value), NULL, &loaded);
    m_pSleeveTexture = pack->FindTexture(name);
}

void CPlayerWeaponObject::Destroy() {
    g_AnimObjFactory.ReleaseObject(this);
}

void CPlayerWeaponObject::InitClass() {}

void SetMuzzleStatusForPlayer(bool) {
    g_scene.GetPlayer(0)->GetCurrentWeapon()->m_muzzleFlash = 1;
}

void CPlayerWeaponObject::SetWeaponName(const char* name) {
    if (name != NULL) {
        strcpy(m_name, name);
        return;
    }
    m_name[0] = 0;
}

// Throws a casing of type `type` out of the "Casing" point (else "Fire"),
// tumbling, carried along by the player's motion.
void CPlayerWeaponObject::CreateEjectedShell(int type) {
    CharSkel_t* skel = m_anim.m_pSkel;
    CThrownObject* shell = CreateSpecialThrownObject(NULL, type, -1);
    if (shell == NULL) {
        return;
    }
    u32 point;
    for (point = 0; point < skel->numMatrices; point++) {
        if (stricmp(skel->matrices[point].name, "Casing") != 0) {
            continue;
        }
        break;
    }
    if (point == skel->numMatrices) {
        for (point = 0; point < skel->numMatrices; point++) {
            if (stricmp(skel->matrices[point].name, "Fire") != 0) {
                continue;
            }
            break;
        }
        if (point == skel->numMatrices) {
            return;
        }
    }
    CMatrix attach;
    attach = *skel->matrices[point].matrix;
    CMatrix tm;
    tm.BuildScale(m_scale);
    tm.Multiply(tm, attach);
    // (The joint is the muzzle's, as in the original.)
    tm.Multiply(tm, m_bones[skel->matrices[m_firePoint].joint]);
    CVector3 pos;
    m_tm.TransformPoint(pos, *(CVector3*)tm.m[3]);
    CVector3 start(pos);

    float vx;
    float vy;
    float vz;
    float rx;
    float ry;
    float rz;
    switch (type) {
    case (int)SHELL_PISTOL:
        vx = 0.016683351f * MathFunRandomReal(-2.0f, 0.0f);
        vy = 0.016683351f * MathFunRandomReal(2.0f, 4.0f);
        vz = 0.016683351f * MathFunRandomReal(2.0f, 3.0f);
        rx = MathFunRandomReal(-0.5f, 0.5f);
        ry = MathFunRandomReal(-0.5f, 0.5f);
        rz = MathFunRandomReal(-0.5f, 0.5f);
        break;
    case (int)SHELL_SHOTGUN:
        vx = 0.016683351f * MathFunRandomReal(0.5f, 1.0f);
        vy = 0.016683351f * MathFunRandomReal(0.0f, 1.0f);
        vz = 0.016683351f * MathFunRandomReal(3.5f, 4.5f);
        rx = MathFunRandomReal(-0.1f, 0.1f);
        ry = MathFunRandomReal(-0.1f, 0.1f);
        rz = MathFunRandomReal(-0.1f, 0.1f);
        break;
    default:
        vx = 0.016683351f * MathFunRandomReal(2.0f, 2.5f);
        vy = 0.016683351f * MathFunRandomReal(0.0f, 0.5f);
        vz = 0.016683351f * MathFunRandomReal(1.0f, 2.0f);
        rx = MathFunRandomReal(-0.5f, 0.5f);
        ry = MathFunRandomReal(-0.5f, 0.5f);
        rz = MathFunRandomReal(-0.5f, 0.5f);
        break;
    }
    CVector3 vel;
    vel.x = vx;
    vel.y = vy;
    vel.z = vz;
    m_tm.TransformVector(vel, vel);
    shell->SetPosition(start);
    CVector3 carried = ((IMovingSceneNode*)m_pPlayer)->GetWorldLinearVelocity();
    vel.x = vel.x + 0.5f * carried.x;
    vel.y = vel.y + 0.5f * carried.y;
    vel.z = vel.z + 0.5f * carried.z;
    shell->SetCollisionId((EClsnId)-1);
    shell->SetVelocity(vel);
    CMatrix spin;
    spin = m_tm;
    spin.RotateX(rx);
    spin.RotateY(ry);
    spin.RotateZ(rz);
    shell->m_spin.SetFromMatrix(spin);
    shell->BeginUpdate(1.0f);
}

int CPlayerWeaponObject::IsDrawEnabled() const {
    int enabled = 0;
    if (m_active && !m_pPlayer->m_hideWeapon) {
        enabled = 1;
    }
    return enabled;
}

// Adds ammo; weapon pickups of this type can be taken while not full.
void CPlayerWeaponObject::AddAmo(int amount) {
    m_pWeapon->AddAmo(amount);
    CWeapon* weapon = m_pWeapon;
    EnableDumpCollisionsByType((EDumpType)1, weapon->m_weaponType,
                               (u32)weapon->m_reserveAmmo < (u32)weapon->m_pProperties->m_maxAmmo);
}

void CPlayerWeaponObject::DoneReloading() {
    m_pWeapon->DoneReloading();
}

void CPlayerWeaponObject::Shoot(EWeaponShootType type, int accuracy) {
    m_pWeapon->Shoot(type, accuracy, -1.0f);
}

// Grenades only while there are any; other weapons once picked up.
bool CPlayerWeaponObject::IsSelectable() const {
    CWeapon* weapon = m_pWeapon;
    int type = weapon->m_weaponType;
    if (type == 0x21 || type == 0x22) {
        return weapon->m_clipAmmo + weapon->m_reserveAmmo != 0;
    }
    return m_selectable;
}

// Draws the weapon with its own field of view (and a near plane close to
// the eye), plus the 2D muzzle flash sprite.
void CPlayerWeaponObject::Draw(CDrawContext& ctx) {
    if (ctx.field_0x114 != (int)m_pPlayer->field_0x768) {
        return;
    }
    if (m_hideFrames > 0) {
        m_hideFrames--;
        return;
    }
    if (g_bInMultiplayerMode && m_pSleeveTexture != NULL) {
        m_anim.BindSleeveTexture(m_pSleeveTexture);
    }
    if (g_config.field_0x3) {
        return;
    }
    if (g_config.field_0x5) {
        return;
    }
    CMatrix proj;
    CMatrix viewProj;
    const CMatrix* sceneProj = ctx.m_pProj;
    viewProj = ctx.m_ViewProj;
    g_Cam2clip = *ctx.m_pProj;
    float fov;
    if (m_pPlayer->m_fov > 0.0f) {
        fov = m_fovScale * (0.017453292f * m_pPlayer->m_fov);
    } else {
        fov = m_fovScale * (0.017453292f * DEFAULT_FOV);
    }

    if ((u8)m_pWeapon->GetMuzzleFlashStatus() && !m_pPlayer->m_scopeView && !g_cheats.field_0xB) {
        // The flash sprite, centred on the projected "Fire" point.
        float left = (float)g_screen.GetWidth() * g_viewport.m_left;
        float top = (float)g_screen.GetHeight() * g_viewport.m_top;
        CVector4 clip;
        g_Cam2clip.Transform(clip, m_flashPos);
        clip.x = clip.x / m_flashPos.z;
        clip.y = clip.y / m_flashPos.z;
        clip.x = clip.x * -320.0f;
        float sy = clip.y * 224.0f;
        clip.y = sy;
        if (g_NumPlayers == 2) {
            sy = 0.5f * sy;
        }
        clip.y = sy;
        float cx = (4.0f + (320.0f + clip.x)) * g_viewport.m_width + left;
        float cy = (224.0f + sy) * g_viewport.m_height + top;
        float size = m_flashScale;
        if (g_bInMultiplayerMode) {
            size *= 0.5f;
        }
        float lo = -96.0f * size;
        float hi = -lo;
        _spritepolyvert verts[4] = {
            {0.0f, 0.0f, 0.0f, 0.0f, 1.0f, {0x80, 0x80, 0x80, 0x55}},
            {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, {0x80, 0x80, 0x80, 0x55}},
            {1.0f, 1.0f, 0.0f, 0.0f, 1.0f, {0x80, 0x80, 0x80, 0x55}},
            {0.0f, 1.0f, 0.0f, 0.0f, 1.0f, {0x80, 0x80, 0x80, 0x55}},
        };
        verts[0].x = lo;
        verts[0].y = lo;
        verts[1].x = hi;
        verts[1].y = lo;
        verts[2].x = hi;
        verts[2].y = hi;
        verts[3].x = lo;
        verts[3].y = hi;
        _spritepolyvert* v = verts;
        for (int i = 0; i < 4; i++, v++) {
            MathFunRotateAboutY(&v->x, &v->y, m_flashSpin);
            v->x = v->x * g_screen.m_widthScale;
            v->x = v->x + cx;
            v->y = v->y * g_screen.m_heightScale;
            v->y = v->y + cy;
        }
        m_pPlayer->m_pMuzzleFlashSprite->DrawPoly(verts, 4, (CSprite::EPolyPrimType)5);
    }

    const CCamera* camera = (const CCamera*)ctx.m_pCamera;
    if (g_NumPlayers != 2) {
        float far = camera->m_far;
        float near = camera->m_near;
        float y = tan(fov * CCamera::ASPECT_4x3);
        proj.Perspective(tan(fov), y, g_fNearMultiplier * near, far);
    } else {
        // Split screen: half height.
        float far = camera->m_far;
        float near = camera->m_near;
        float y = tan(CCamera::ASPECT_4x3 * (0.75f * fov));
        proj.Perspective(tan(1.3333334f * fov), y, g_fNearMultiplier * near, far);
    }
    ctx.m_pProj = &proj;
    ctx.m_ViewProj.Multiply(*ctx.m_pView, *ctx.m_pProj);
    ctx.m_pWorld = &m_tm;
    m_anim.UpdateSkeletonMatrices(m_bones, NULL, NULL);
    bool draw = true;
    // (The scale and the word after it are saved as two doubles.)
    f64 savedScale0 = ((f64*)ctx.m_scale)[0];
    f64 savedScale1 = ((f64*)ctx.m_scale)[1];
    ctx.m_scale[0] = m_scale.x;
    ctx.m_scale[1] = m_scale.y;
    ctx.m_scale[2] = m_scale.z;
    if (m_pPlayer->m_zoomed) {
        int mode = g_cheats.field_0x2 ? 3 : m_hideMode;
        if (mode == 3) {
            if (!m_pWeapon->m_bReloading || m_pWeapon->m_reserveAmmo == 0) {
                draw = false;
            }
        } else if (m_pSprite0 != NULL && (!m_pWeapon->m_bReloading || m_pWeapon->m_reserveAmmo == 0)) {
            draw = false;
        }
    }
    if (!g_scene.GetPlayer(0)->m_ui.m_letterbox && draw) {
        m_anim.Draw(ctx);
    }
    ctx.m_pProj = sceneProj;
    ctx.m_ViewProj = viewProj;
    ((f64*)ctx.m_scale)[0] = savedScale0;
    ((f64*)ctx.m_scale)[1] = savedScale1;
}

int CPlayerWeaponObject::CommitUpdate() { END_RETURN_NOTHING }

int CPlayerWeaponObject::AttemptUpdate(float) { END_RETURN_NOTHING }

// Animates the arms, follows the player and, on a shot, spins the flash and
// spawns the muzzle particles. Also keeps weapon pickups of this type
// collectable only while there is room for more ammo.
int CPlayerWeaponObject::BeginUpdate(float dt) {
    CWeapon* weapon = m_pWeapon;
    if (weapon != NULL && !g_bInMultiplayerMode) {
        s16 reserve = weapon->m_reserveAmmo;
        if (reserve != m_lastReserve) {
            EnableDumpCollisionsByType((EDumpType)1, weapon->m_weaponType,
                                       (u32)reserve < (u32)weapon->m_pProperties->m_maxAmmo);
            m_lastReserve = reserve;
        }
    }
    if (!m_active) {
        RETURN_NOTHING;
    }
    UpdateAnimation(dt);
    if (m_pPlayer->m_hideWeapon) {
        RETURN_NOTHING;
    }
    UpdateWeaponTransforms();
    m_pWeapon->Update(dt, false);
    if (!(u8)m_pWeapon->GetMuzzleFlashStatus() || m_pPlayer->m_scopeView) {
        RETURN_NOTHING;
    }
    m_flashSpin = 0.0f;
    m_flashSpin = MathFunRandomReal(0.0f, 6.28f * dt);
    float scale = m_flashScale;
    if (m_pMuzzleFlash == NULL) {
        RETURN_NOTHING;
    }
    CharSkel_t* skel = m_anim.m_pSkel;
    CMatrix scaleTM;
    CMatrix fire;
    CMatrix tm;
    scaleTM.BuildScale(scale);
    fire = m_fireTM;
    fire.RotateY(m_flashSpin);
    fire.Multiply(scaleTM, fire);
    tm.Multiply(fire, m_bones[skel->matrices[m_firePoint].joint]);
    scaleTM.BuildScale(m_scale);
    tm.Multiply(tm, scaleTM);
    tm.Multiply(tm, m_tm);
    u32 crc = PARTICLES_MUZZLE_FLASH;
    if (m_flashParticles != 0) {
        crc = m_flashParticles;
    }
    if (!g_bInMultiplayerMode) {
        CParticleSystem* ps = CParticleSystem::Create(crc);
        if (ps != NULL) {
            ps->SetLocalToWorld(tm);
        }
    }
    END_RETURN_NOTHING
}

// Places the weapon at the player's eyes (plus its offset, rotation and the
// player's weapon sway), aims the weapon along the camera and finds the
// muzzle of weapons with a flash.
void CPlayerWeaponObject::UpdateWeaponTransforms() {
    ((ISceneNode*)m_pPlayer)->GetTMLocalToWorld(m_tm);
    CMatrix eye;
    eye = m_pPlayer->m_eyeTransform;
    m_tm.Multiply(eye, m_tm);
    CMatrix local;
    CVector3 offset(m_offset);
    offset.y += 1.0f;
    local.BuildTrans(offset);
    local.RotateX(m_pitch + m_pPlayer->field_0x44C);
    local.RotateY(m_yaw);
    local.RotateZ(m_roll + m_pPlayer->field_0x450);
    m_tm.Multiply(local, m_tm);
    m_inView = m_pPlayer->m_zoomed;
    CCamera* camera = &m_pPlayer->m_camera;
    CVector3 aim;
    CVector3 eyePos;
    camera->GetForward(aim);
    camera->GetPosition(eyePos);
    m_pWeapon->m_aimOrigin = eyePos;
    m_pWeapon->m_aimDir = aim;
    switch (m_pWeapon->m_weaponType) {
    case 5:
    case 6:
    case 7:
    case 8:
    case 14:
    case 15:
    case 16:
    case 20:
    case 21:
    case 24:
    case 25:
    case 29:
    case 30:
    case 33:
    case 34:
    case 35:
    case 36: {
        m_hasFlash = 1;
        m_pMuzzleFlash = g_pStaticObjectMuzzleFlashPlayer;
        CharSkel_t* skel = m_anim.m_pSkel;
        u32 point;
        for (point = 0; point < skel->numMatrices; point++) {
            if (stricmp(skel->matrices[point].name, "Fire") != 0) {
                continue;
            }
            break;
        }
        if (point == skel->numMatrices) {
            point = 0;
        }
        m_firePoint = point;
        m_fireTM = *skel->matrices[point].matrix;
        break;
    }
    default:
        m_hasFlash = 0;
        m_pMuzzleFlash = NULL;
        break;
    }
    if (m_hasFlash) {
        // The muzzle in camera space (for the flash sprite).
        int joint = m_anim.m_pSkel->matrices[m_firePoint].joint;
        CMatrix tm;
        tm.BuildScale(m_scale);
        tm.Multiply(tm, m_fireTM);
        tm.Multiply(tm, m_bones[joint]);
        tm.Multiply(tm, m_tm);
        tm.Multiply(tm, camera->m_worldToCamera);
        m_flashPos = *(CVector3*)tm.m[3];
    }
}

void CPlayerWeaponObject::Shutdown() {
    if (m_pWeapon != NULL) {
        g_WeaponFactory.DestroyWeapon(m_pWeapon);
    }
    g_scene.Remove(*this);
}

void CPlayerWeaponObject::Init(EWeaponTypes type, int ammo, CPlayerObject* player) {
    m_pSleeveTexture = NULL;
    m_pPlayer = player;
    m_tm.Ident();
    m_inView = 0;
    m_active = 0;
    m_selectable = 0;
    m_hasFlash = 0;
    m_lastReserve = -1;
    for (int i = 0; i < NUM_BONES; i++) {
        m_bones[i].Ident();
    }
    m_pWeapon = g_WeaponFactory.CreateWeapon(type, (ISceneNode*)player, ammo);
    field_0x2452 = 1;
    g_scene.Add(*this);
    m_pMuzzleFlash = NULL;
    m_flashPos = CVector3(0.0f, 0.0f, 0.0f);
    m_hideFrames = 0;
}

CPlayerWeaponObject::~CPlayerWeaponObject() {}

CPlayerWeaponObject::CPlayerWeaponObject() {
    m_pPrevAnim = NULL;
    m_pNextAnim = NULL;
    m_pNextFree = NULL;
    m_pWeapon = NULL;
    m_pLocalBound = NULL;
    m_pLocalVolume = NULL;
    m_pWorldBound = NULL;
    m_pWorldVolume = NULL;
    m_pScriptObject = NULL;
    m_pSprite0 = NULL;
    m_pSprite2 = NULL;
    m_pMuzzleFlash = NULL;
    m_collisionId = -1;
}
