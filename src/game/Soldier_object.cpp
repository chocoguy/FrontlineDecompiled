// CSoldierObject: the soldier characters (see Soldier_object.h).
#include "game/Soldier_object.h"
#include "game/animated_volume.h"
#include "game/animmot.h"
#include "game/box.h"
#include "game/bullet.h"
#include "game/collision.h"
#include "game/SClsnContact.h"
#include "game/draw_context.h"
#include "game/projectilebullet.h"
#include "game/thrownbullet.h"
#include "game/thrown_obj.h"
#include "game/weapon.h"
#include "game/weaponfactory.h"
#include "game/mainloop.h"
#include "game/staticmesh.h"
#include "game/staticobjfactory.h"
#include "game/objcreate.h"
#include "game/particlesystem.h"
#include "game/player.h"
#include "game/scene.h"
#include "game/sound.h"
#include "game/light.h"
#include "game/gameconfig.h"
#include "game/ShellMenu.h"
#include "game/AIFilter.h"
#include "game/AIObject.h"
#include "game/MathFun.h"
#include "game/Moh2.h"
#include "game/bsevents.h"
#include "game/bsobject.h"
#include "game/BSGO_Basic.h"
#include "game/bstypes.h"
#include "math.h"
#include "stdlib.h"
#include "string.h"

// The mesh hard point bullets and muzzle flashes come from.
#define HARDPOINT_MUZZLE 0xE6E804D5
// AddWeapon: the CRC of "no weapon".
#define WEAPON_CRC_NONE 0xFB9630C9

// Head tracking keeps the look direction in front of the body and within
// 60 degrees of level.
static bool g_bConstraint = true;

void CSoldierObject::Destroy() {
    if (m_pScriptObject != NULL && m_pScriptObject->m_gameObject != NULL) {
        m_pScriptObject->m_gameObject->Destroy();
    }
}

void CSoldierObject::SetHeadTrans(bool on) {
    if (m_headTrans <= 1) {
        m_headTrans = on;
    } else if (m_headTrans == 2 && !on) {
        m_headTrans = 3;
    }
}

void CSoldierObject::SetHeadTrackPlayer(bool on, int steps) {
    if (on != m_headTrackPlayer) {
        m_headTrackPlayer = on;
        m_headTrackSteps = steps;
        m_headTrackStep = 1;
    }
}

// Stops moving and fades out over `duration` (dropping the weapons
// three quarters of the way through).
void CSoldierObject::FadeOut(float duration) {
    m_fading = 1;
    m_fadeTime = 0.0f;
    m_fadeDuration = duration;
    m_velocity.z = 0.0f;
    m_velocity.y = 0.0f;
    m_velocity.x = 0.0f;
    m_fadeDropped = 0;
}

// Bullets of the current weapon leave from the muzzle hard point of `gun`,
// held on attach point `name`.
void CSoldierObject::SetBulletEmitter(CStaticObject* gun, char* name) {
    if (m_pWeapons[m_curWeapon]->m_pMuzzleFlash == NULL) {
        m_hasEmitter = 0;
        return;
    }
    CharSkel_t* skel = m_anim.m_pSkel;
    u32 point;
    for (point = 0; point < skel->numMatrices; point++) {
        if (stricmp(skel->matrices[point].name, name) == 0) {
            break;
        }
    }
    StaticMesh* mesh = gun->m_pMesh->m_pData;
    int n = mesh->m_numHardPoints;
    if (n > 0) {
        StaticMeshHardPoint* hp = mesh->m_hardPoints;
        int i;
        for (i = 0; i < n; i++, hp++) {
            if (hp->m_id == HARDPOINT_MUZZLE) {
                m_emitterPoint = point;
                m_emitterTM = hp->m_tm;
                m_hasEmitter = 1;
                break;
            }
        }
        if (i >= n) {
            m_emitterPoint = 0;
            m_hasEmitter = 0;
        }
    }
}

void CSoldierObject::DetachFloatingBone() {
    if (m_pFloatingObject != NULL) {
        m_floatingBoneOn = 0;
    }
}

void CSoldierObject::AttachFloatingBone() {
    if (m_pFloatingObject != NULL) {
        m_floatingBoneOn = 1;
    }
}

void CSoldierObject::DetachObjectFromFloatingBone() {
    m_floatingBoneOn = 0;
    m_pFloatingObject = NULL;
}

// `obj` follows the animation's floating bone, held at attach point `name`
// (or at its own hard point `hardPoint`).
int CSoldierObject::AttachObjectToFloatingBone(CStaticObject* obj, char* name, int hardPoint) {
    CharSkel_t* skel = m_anim.m_pSkel;
    if (obj == NULL) {
        return 0;
    }
    u32 point;
    for (point = 0; point < skel->numMatrices; point++) {
        if (stricmp(skel->matrices[point].name, name) == 0) {
            break;
        }
    }
    if (point == skel->numMatrices) {
        return 0;
    }
    if (m_pFloatingObject != NULL) {
        return 0;
    }
    m_pFloatingObject = obj;
    CMatrix inv;
    if (hardPoint != 0) {
        StaticMesh* mesh = obj->m_pMesh->m_pData;
        int n = mesh->m_numHardPoints;
        StaticMeshHardPoint* hp = mesh->m_hardPoints;
        for (int i = 0; i < n; i++, hp++) {
            if (hp->m_id == hardPoint) {
                i = n;
                inv.Inverse(hp->m_tm);
            }
        }
        m_floatingTM = inv;
    } else {
        m_floatingTM = *skel->matrices[point].matrix;
    }
    return 1;
}

int CSoldierObject::GetAttachPointMatrix(char* name, CMatrix* out) {
    CharSkel_t* skel = m_anim.m_pSkel;
    u32 point;
    for (point = 0; point < skel->numMatrices; point++) {
        if (stricmp(skel->matrices[point].name, name) == 0) {
            break;
        }
    }
    if (point == skel->numMatrices) {
        return 0;
    }
    *out = *skel->matrices[point].matrix;
    return 1;
}

// Moves the helmet by `offset` (in its own space).
void CSoldierObject::SetHelmetOffset(CVector3& offset) {
    u32 point = FindAttachPoint("head");
    CMatrix& tm = m_attachTM[point];
    CVector3 pos(*(CVector3*)tm.m[3]);
    CVector3 d;
    tm.TransformVector(d, offset);
    pos.x += d.x;
    pos.y += d.y;
    pos.z += d.z;
    tm.SetPos(pos);
}

// Puts a bare mesh on attach point `name` (at its own hard point
// `hardPoint`, if given).
int CSoldierObject::AttachMesh(CStaticMesh* mesh, char* name, int hardPoint) {
    CharSkel_t* skel = m_anim.m_pSkel;
    if (mesh == NULL) {
        return 0;
    }
    u32 point = FindAttachPoint(name);
    if (point == (u32)-1) {
        return 0;
    }
    if (m_attachType[point] != SOLDIER_ATTACH_NONE) {
        return 0;
    }
    m_attached[point] = mesh;
    m_attachType[point] = SOLDIER_ATTACH_MESH;
    ((CStaticMesh*)m_attached[point])->EnableLighting(true);
    CMatrix inv;
    if (hardPoint != 0) {
        StaticMesh* data = mesh->m_pData;
        int n = data->m_numHardPoints;
        StaticMeshHardPoint* hp = data->m_hardPoints;
        for (int i = 0; i < n; i++, hp++) {
            if (hp->m_id == hardPoint) {
                i = n;
                inv.Inverse(hp->m_tm);
            }
        }
        m_attachTM[point].Multiply(inv, *skel->matrices[point].matrix);
    } else {
        m_attachTM[point] = *skel->matrices[point].matrix;
    }
    return 1;
}

void CStaticMesh::EnableLighting(bool on) {
    m_lighting = on;
}

CStaticObject* CSoldierObject::DetachObject(char* name) {
    CharSkel_t* skel = m_anim.m_pSkel;
    u32 point;
    for (point = 0; point < skel->numMatrices; point++) {
        if (stricmp(skel->matrices[point].name, name) == 0) {
            break;
        }
    }
    if (point == skel->numMatrices) {
        return NULL;
    }
    CStaticObject* obj = (CStaticObject*)m_attached[point];
    m_attached[point] = NULL;
    m_attachType[point] = SOLDIER_ATTACH_NONE;
    if (m_emitterPoint == point) {
        m_hasEmitter = 0;
    }
    return obj;
}

// Puts `obj` on attach point `name` (at its own hard point `hardPoint`,
// if given).
bool CSoldierObject::AttachObject(CStaticObject* obj, char* name, int hardPoint) {
    CharSkel_t* skel = m_anim.m_pSkel;
    if (obj == NULL) {
        return false;
    }
    u32 point = FindAttachPoint(name);
    if (point == (u32)-1) {
        return false;
    }
    if (m_attachType[point] != SOLDIER_ATTACH_NONE) {
        return false;
    }
    m_attached[point] = obj;
    m_attachType[point] = SOLDIER_ATTACH_OBJECT;
    CStaticMesh* mesh = obj->m_pMesh;
    if (mesh != NULL) {
        mesh->EnableLighting(true);
    }
    CMatrix inv;
    if (hardPoint != 0) {
        // (Like the original, assumes the object has a mesh.)
        StaticMesh* data = mesh->m_pData;
        int n = data->m_numHardPoints;
        StaticMeshHardPoint* hp = data->m_hardPoints;
        int i;
        for (i = 0; i < n; i++, hp++) {
            if (hp->m_id == hardPoint) {
                inv.Inverse(hp->m_tm);
                break;
            }
        }
        CMatrix& tm = m_attachTM[point];
        tm = *skel->matrices[point].matrix;
        if (i < n) {
            tm.Multiply(inv, tm);
        }
    } else {
        m_attachTM[point] = *skel->matrices[point].matrix;
    }
    return true;
}

// Fires the mounted gun `gun` at `target`, at most 10 degrees off its
// barrel.
bool CSoldierObject::FireMMG(CVector3& target, CStaticObject* gun) {
    StaticMesh* data = gun->m_pMesh->m_pData;
    int n = data->m_numHardPoints;
    StaticMeshHardPoint* hp = data->m_hardPoints;
    CMatrix tm;
    tm = gun->m_tm;
    for (int i = 0; i < n; i++, hp++) {
        if (hp->m_id == HARDPOINT_MUZZLE) {
            tm.Multiply(hp->m_tm, tm);
            break;
        }
    }
    CVector3 origin(*(CVector3*)tm.m[3]);
    CVector3 dir(target);
    dir.x = dir.x - origin.x;
    dir.y -= origin.y;
    dir.z = dir.z - origin.z;
    dir.Normalize();
    CVector3 barrel(*(CVector3*)tm.m[1]);
    float d = barrel.z * dir.z + (barrel.x * dir.x + barrel.y * dir.y);
    if (d < 0.9848f) {
        // Turn from the barrel towards the target by the largest angle allowed.
        float limit = acos(0.9848f);
        float angle = acos(d);
        float s = sin(angle);
        float kBarrel = sin(angle - limit) / s;
        barrel.x *= kBarrel;
        float kDir = sin(limit) / s;
        dir.x *= kDir;
        dir.y *= kDir;
        barrel.y *= kBarrel;
        dir.z *= kDir;
        barrel.z *= kBarrel;
        dir.x = dir.x + barrel.x;
        dir.y = dir.y + barrel.y;
        dir.z = dir.z + barrel.z;
    }
    m_pMMGWeapon->m_aimDir = dir;
    m_pMMGWeapon->m_aimOrigin = origin;
    m_pMMGWeapon->Shoot((EWeaponShootType)2, 0.0f, -1.0f);
    m_mmgFired = 1;
    m_pMMG = gun;
    return true;
}

// Fires weapon `slot` along `dir`: from the held gun's muzzle (at most 10
// degrees off its barrel) or from the chest (at most 40 degrees off the
// soldier's facing).
bool CSoldierObject::FireWeapon(int slot, CVector3& from, CVector3& dir, float accuracy) {
    if (m_pWeapons[m_curWeapon] == NULL) {
        DebugMsg("WEAPON WARNING WEAPON WARNING WEAPON WARNING\n");
        DebugMsg("WEAPON WARNING WEAPON WARNING WEAPON WARNING\n");
        DebugMsg("WEAPON WARNING WEAPON WARNING WEAPON WARNING\n");
        DebugMsg("A person is trying to fire a weapon from script although the person's property\n");
        DebugMsg("data says that the person is not supposed to have a weapon!!\n");
        DebugMsg("Persons Class: %s", m_pScriptObject->m_class->m_name);
        TriggerObjectData_struct* data = m_pScriptObject->m_trigger->m_data;
        DebugMsg("Persons Trigger Location: x: %d, y: %d, z: %d\n", (int)data->m_position[0],
                 (int)data->m_position[1], (int)data->m_position[2]);
        return false;
    }
    if (m_hasEmitter) {
        CMatrix tm;
        m_pWeapons[m_curWeapon]->m_muzzleFlash = 1;
        if (m_hasEmitter && m_attachType[m_emitterPoint] != SOLDIER_ATTACH_NONE) {
            GetAttachPointTM(tm, m_emitterPoint);
            tm.Multiply(m_emitterTM, tm);
        } else {
            tm = m_upperTM;
        }
        CVector3 barrel(*(CVector3*)tm.m[1]);
        CVector3 aim(dir);
        CVector3 origin(*(CVector3*)tm.m[3]);
        aim.Normalize();
        aim.Constrain(barrel, 0.9848f);
        CWeapon** weapon = &m_pWeapons[slot];
        (*weapon)->m_aimDir = aim;
        (*weapon)->m_aimOrigin = origin;
        (*weapon)->Shoot((EWeaponShootType)2, accuracy, -1.0f);
        return true;
    }
    CMatrix chest;
    chest = m_bones[15];
    CVector3 origin(*(CVector3*)chest.m[3]);
    m_tm.TransformPoint(origin, origin);
    CVector3 aim(dir);
    aim.Normalize();
    CVector3 forward;
    GetForward(forward);
    aim.Constrain(forward, 0.766f);
    CWeapon** weapon = &m_pWeapons[slot];
    (*weapon)->m_aimDir = aim;
    (*weapon)->m_aimOrigin = origin;
    (*weapon)->Shoot((EWeaponShootType)2, accuracy, -1.0f);
    return true;
}

// The held gun's muzzle (see SetBulletEmitter), else the upper body.
void CSoldierObject::GetBulletEmitterMatrix(CMatrix& out) {
    if (m_hasEmitter && m_attachType[m_emitterPoint] != SOLDIER_ATTACH_NONE) {
        GetAttachPointTM(out, m_emitterPoint);
        out.Multiply(m_emitterTM, out);
        return;
    }
    out = m_upperTM;
}

// Knocks the helmet off along `dir` (scaled down, with a random upward
// kick). Returns whether it was a helmet that stops bullets.
int CSoldierObject::EjectHelmet(CVector3* dir) {
    int stops = 1;
    u32 point = FindAttachPoint("head");
    if (point == (u32)-1) {
        return 0;
    }
    if (m_attachType[point] == SOLDIER_ATTACH_NONE) {
        return 0;
    }
    int type = m_pScriptObject->m_trigger->m_data->field_0x48;
    CThrownObject* helmet = CreateSpecialThrownObject(NULL, type, -1);
    if (helmet == NULL) {
        return 0;
    }
    if (!(u8)CStaticObjectFactory::DoesHelmetTypeStopBullets(type)) {
        stops = 0;
    }
    CMatrix tm;
    GetAttachPointTM(tm, point);
    helmet->SetTMLocalToWorld(tm);
    float s = MathFunRandomReal(0.002f, 0.01f);
    dir->x = dir->x * s;
    dir->y = dir->y * s;
    dir->z = dir->z * s;
    dir->z = MathFunRandomReal(0.005f, 0.065f);
    helmet->SetVelocity(*dir);
    helmet->BeginUpdate(1.0f);
    CharSkel_t* skel = m_anim.m_pSkel;
    u32 head;
    for (head = 0; head < skel->numMatrices; head++) {
        if (stricmp(skel->matrices[head].name, "head") == 0) {
            break;
        }
    }
    if (head != skel->numMatrices) {
        m_attached[head] = NULL;
        m_attachType[head] = SOLDIER_ATTACH_NONE;
    }
    return stops;
}

void CBullet::GetVelocity(CVector3&) {}

// Hit by a bullet: script event 0x2B with the damage for the hit location
// (head shots x3 unless a helmet takes them, limbs x0.5), the player's hit
// statistics, an impact sound and a blood puff. Grenades explode.
void CSoldierObject::HandleBulletCollision(CBullet* bullet, const CCollision& clsn) {
    CProjectileBullet* projectile = bullet->AsProjectile();
    CThrownBullet* thrown = bullet->AsThrown();
    if (projectile == NULL) {
        if (thrown != NULL) {
            thrown->Explode();
        }
        return;
    }
    BS_STRUCT_HitByBulletInfo_struct* event =
        (BS_STRUCT_HitByBulletInfo_struct*)BSEventGetEventMemoryBlock(sizeof(BS_STRUCT_HitByBulletInfo_struct));
    event->m_bulletType = bullet->GetScriptBulletType();
    event->m_damage = bullet->GetDamage();
    if (event->m_bulletType == 1 && m_burstTimer == 0.0f) {
        m_burstTimer = 0.25f;
    }
    if (m_burstTimer > 0.0f) {
        event->m_burstHit = ++m_burstHits;
    } else {
        event->m_burstHit = 1;
    }
    ISceneNode* firedBy = bullet->GetFiredBy();
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
    float alongForward = forward.z * dir.z + (forward.x * dir.x + forward.y * dir.y);
    if (alongRight < -0.7071f) {
        event->m_side = 3;
    } else if (alongRight > 0.7071f) {
        event->m_side = 2;
    } else if (alongForward < 0.0f) {
        event->m_side = 0;
    } else {
        event->m_side = 1;
    }
    const SClsnContact* contact = clsn.m_pContacts;
    u8 helmetHit = 0;
    event->m_pPoly = contact->m_pPoly;
    int location = (int)PTR_TO_ADDR(event->m_pPoly);
    if (location < 9) {
        if (location == 2) {
            event->m_damage = (float)event->m_damage;
        } else if (location < 2) {
            if (location >= 0) {
                // Head: a helmet may take the shot (and fly off).
                float scale = 1.0f;
                if (MathFunRandomI64(0, 100) < m_helmetChance) {
                    u8 stopped = 1;
                    u32 point = FindAttachPoint("head");
                    if (point == (u32)-1) {
                        stopped = 0;
                    } else if (m_attachType[point] == SOLDIER_ATTACH_NONE) {
                        stopped = 0;
                    } else {
                        int type = m_pScriptObject->m_trigger->m_data->field_0x48;
                        CThrownObject* helmet = CreateSpecialThrownObject(NULL, type, -1);
                        if (helmet == NULL) {
                            stopped = 0;
                        } else {
                            CharSkel_t* skel = m_anim.m_pSkel;
                            u32 head;
                            for (head = 0; head < skel->numMatrices; head++) {
                                if (stricmp(skel->matrices[head].name, "head") == 0) {
                                    break;
                                }
                            }
                            if (head != skel->numMatrices) {
                                m_attached[head] = NULL;
                                m_attachType[head] = SOLDIER_ATTACH_NONE;
                            }
                            if (!(u8)CStaticObjectFactory::DoesHelmetTypeStopBullets(type)) {
                                stopped = 0;
                                helmet->m_playSounds = 2;
                            } else {
                                helmet->m_playSounds = 1;
                            }
                            CMatrix tm;
                            GetAttachPointTM(tm, point);
                            helmet->SetTMLocalToWorld(tm);
                            CVector3 vel;
                            bullet->GetVelocity(vel);
                            float s = MathFunRandomReal(0.002f, 0.01f);
                            vel.x *= s;
                            vel.y *= s;
                            vel.z *= s;
                            vel.z = MathFunRandomReal(0.005f, 0.065f);
                            helmet->SetVelocity(vel);
                            helmet->BeginUpdate(1.0f);
                        }
                    }
                    helmetHit = stopped;
                    if (stopped) {
                        scale = m_helmetHitScale;
                    }
                }
                event->m_damage = scale * (3.0f * event->m_damage);
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
    if (g_cheats.field_0xC && m_pScriptObject->m_trigger->m_data->m_team == 2) {
        event->m_damage = 1000;
    }
    if (g_cheats.field_0xD) {
        int loc = (int)PTR_TO_ADDR(event->m_pPoly);
        if (loc != 0 && loc != 1) {
            event->m_damage = 0;
        }
    }
    BSObjectTriggerEvent(m_pScriptObject, 0x2B, event, NULL, false);
    if (firedBy != NULL && firedBy->AsPlayerObject() != NULL &&
        m_pScriptObject->m_trigger->m_data->m_team == 2 && event->m_burstHit < 2) {
        g_Shell.Set_hits(1, false);
        switch ((int)PTR_TO_ADDR(event->m_pPoly)) {
        case 3:
        case 4:
        case 5:
            g_Shell.Set_shotLocation(1, (EShotLocations)1, false);
            break;
        case 6:
        case 7:
        case 8:
            g_Shell.Set_shotLocation(1, (EShotLocations)2, false);
            break;
        case 13:
        case 14:
        case 15:
            g_Shell.Set_shotLocation(1, (EShotLocations)4, false);
            break;
        case 16:
        case 17:
        case 18:
            g_Shell.Set_shotLocation(1, (EShotLocations)5, false);
            break;
        case 19:
        case 20:
            g_Shell.Set_shotLocation(1, (EShotLocations)6, false);
            break;
        case 0:
        case 1:
            g_Shell.Set_shotLocation(1, (EShotLocations)0, false);
            break;
        case 2:
        case 9:
        case 10:
        case 11:
        case 12:
            g_Shell.Set_shotLocation(1, (EShotLocations)3, false);
            break;
        }
    }
    PlayImpactSound(helmetHit ? 0xF : 0xE, 1, contact->m_point, false);
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

// m_tm moved to the "VisionCone" attach point, if the skeleton has one.
void CSoldierObject::GetLookMatrix(CMatrix& out) {
    u32 point = FindAttachPoint("VisionCone");
    out = m_tm;
    if (point != (u32)-1) {
        CharSkelMatrixEntry_t* entry = &m_anim.m_pSkel->matrices[point];
        out.Multiply(m_bones[entry->joint], out);
        out.Multiply(*entry->matrix, out);
    }
}

// Replaces weapon `slot` with the weapon called `crc`.
int CSoldierObject::AddWeapon(int slot, int crc) {
    CWeapon** weapon = &m_pWeapons[slot];
    if (*weapon != NULL) {
        g_WeaponFactory.DestroyWeapon(*weapon);
    }
    if ((u32)crc == WEAPON_CRC_NONE) {
        *weapon = NULL;
        return 1;
    }
    *weapon = g_WeaponFactory.CreateWeaponByCRC(crc, this, 999);
    return *weapon != NULL;
}

void CSoldierObject::SetAttachedLight(CLight* light, CVector3 offset) {
    if (light != NULL) {
        CMatrix tm;
        GetTMLocalToWorld(tm);
        tm.PreTranslate(offset);
        light->SetTMLocalToWorld(tm);
    }
}

// Touching another character raises a script event that depends on both
// collision classes (and pushes the player away); bumping into the world
// or into objects raises 0xA9, 0x85 (standing on it) or 0xBF.
void CSoldierObject::OnCollision(const CCollision& clsn) {
    CAnimObject::OnCollision(clsn);
    ISceneNode* other = clsn.m_pNode0;
    if (other == this) {
        other = clsn.m_pNode1;
    }
    if (other->AsSoldierObject() != NULL || other->AsPlayerObject() != NULL) {
        int myId = GetCollisionId();
        int otherId = other->GetCollisionId();
        int myClass = -1;
        int otherClass = -1;
        // Collision id -> class (0, 1 or 2).
        int classes[7][2] = {
            {2, 0}, {0xD, 0}, {1, 1}, {5, 1}, {7, 2}, {0x19, 2}, {-1, 2},
        };
        for (int i = 0; i < 7; i++) {
            if (myId == classes[i][0]) {
                myClass = classes[i][1];
            }
            if (otherId == classes[i][0]) {
                otherClass = classes[i][1];
            }
        }
        // (my class, other's class) -> event.
        int events[9][3] = {
            {0, 0, 0xBD}, {0, 1, 0xBC}, {0, 2, 0xBE},
            {1, 0, 0xBC}, {1, 1, 0xBD}, {1, 2, 0xBE},
            {2, 0, 0xBE}, {2, 1, 0xBE}, {2, 2, 0xBE},
        };
        int event = -1;
        for (int i = 0; i < 9; i++) {
            if (myClass == events[i][0] && otherClass == events[i][1]) {
                event = events[i][2];
                break;
            }
        }
        if (other->AsPlayerObject() == NULL) {
            BSObjectTriggerEvent(m_pScriptObject, event, other->GetScriptObject(), NULL, true);
        }
        ISceneNode* player = (ISceneNode*)other->AsPlayerObject();
        if (player != NULL) {
            if (player->GetAIDoodad()->m_pFilter->m_pAIObject->m_status.GetVarValue((aistatus_var)5) == 1) {
                BSObjectTriggerEvent(m_pScriptObject, event, other->GetScriptObject(), NULL, true);
            }
            // Push the player out along the contact normals, harder the
            // more vertical they are.
            CVector3 push;
            push.z = 0.0f;
            push.y = 0.0f;
            push.x = 0.0f;
            for (const SClsnContact* c = clsn.m_pContacts; c != NULL; c = c->m_pNext) {
                push.x += c->m_normal.x;
                push.y += c->m_normal.y;
                push.z += c->m_normal.z;
            }
            if (!(clsn.m_pNode0 == this)) {
                push.x *= -1.0f;
                push.y *= -1.0f;
                push.z *= -1.0f;
            }
            float strength = 30.0f * (float)fabs(push.z);
            push.z = 0.0f;
            if (strength > 1.0f) {
                if (sqrtf(push.x * push.x + push.y * push.y + 0.0f * 0.0f) > 0.0001f) {
                    push.Normalize();
                    other->AsMovingNode()->ApplyForceTo(push, strength);
                }
            }
        }
    } else if (other->AsWorldObject() != NULL) {
        BSObjectTriggerEvent(m_pScriptObject, 0xA9, (void*)&clsn, NULL, true);
    } else if (other->AsStaticObject() != NULL || other->AsHierObject() != NULL) {
        u8 onTop = 0;
        // (Like the original, steps through the contacts as an array.)
        for (const SClsnContact* c = clsn.m_pContacts; c != NULL; c++) {
            if (c->m_normal.z * 1.0f + (c->m_normal.x * 0.0f + c->m_normal.y * 0.0f) > 0.8f) {
                onTop = 1;
                break;
            }
        }
        if (onTop) {
            BSObjectTriggerEvent(m_pScriptObject, 0x85, (void*)&clsn, NULL, true);
            return;
        }
        BSObjectTriggerEvent(m_pScriptObject, 0xBF, (void*)&clsn, NULL, true);
    }
}

const IVolume* CSoldierObject::GetWorldBoundingVolume(ISceneNode::EVolumeType type) const {
    switch ((int)type) {
    case 3:
        return m_pWorldBound;
    case 0:
        return m_pWorldVolume;
    case 1:
        return &m_worldSphere1;
    case 2:
        return &m_worldSphere2;
    case 4:
    default:
        return NULL;
    }
}

const IVolume* CSoldierObject::GetLocalBoundingVolume(ISceneNode::EVolumeType type) const {
    switch ((int)type) {
    case 3:
        return m_pLocalBound;
    case 0:
        return m_pLocalVolume;
    case 1:
        return &m_localSphere1;
    case 2:
        return &m_localSphere2;
    case 4:
    default:
        return NULL;
    }
}

void CSoldierObject::UpdateLocalBoundingVolume(ISceneNode::EVolumeType type, const CVector3& min,
                                               const CVector3& max) {
    switch ((int)type) {
    case 3:
        ((CVolBox*)m_pLocalBound)->SetCorners(min, max);
        break;
    }
}

int CSoldierObject::CommitUpdate() {
    return CAnimObject::CommitUpdate();
}

int CSoldierObject::AttemptUpdate(float dt) {
    CAnimObject::AttemptUpdate(dt);
    m_worldSphere1.TransformedCopy(m_localSphere1, m_tm);
    m_worldSphere2.TransformedCopy(m_localSphere2, m_tm);
    END_RETURN_NOTHING
}

// Moves the upper body matrix along and runs the AI.
void CSoldierObject::UpdateAI(float dt) {
    CVector3 right(*(CVector3*)m_upperTM.m[0]);
    CVector3 forward(*(CVector3*)m_upperTM.m[1]);
    CVector3 up(*(CVector3*)m_upperTM.m[2]);
    CVector3 pos;
    GetPosition(pos);
    right.Scale(right, m_headPosition[0]);
    forward.Scale(forward, m_headPosition[1]);
    up.Scale(up, m_headPosition[2]);
    pos.Add(right);
    pos.Add(forward);
    pos.Add(up);
    m_upperTM.SetPos(pos);
    m_aiDoodad.Update(dt);
}

int CSoldierObject::BeginUpdate(float dt) {
    float seconds = dt / 60.0f;
    CAnimObject::BeginUpdate(dt);
    m_aiDoodad.PreUpdate();
    if (m_pWeapons[m_curWeapon] != NULL) {
        m_pWeapons[m_curWeapon]->Update(dt, false);
        m_pMMGWeapon->Update(dt, false);
    }
    if (m_fading) {
        m_velocity.z = 0.0f;
        m_velocity.y = 0.0f;
        m_velocity.x = 0.0f;
        m_fadeTime += seconds;
    }
    if (m_burstTimer > 0.0f) {
        m_burstTimer = m_burstTimer - seconds;
    } else if (m_burstTimer < 0.0f) {
        m_burstTimer = 0.0f;
        m_burstHits = 0;
    }
    END_RETURN_NOTHING
}

// The weapon's muzzle flash at the held gun's muzzle (or the mounted gun's),
// randomly spun about the barrel. Flashes on alternate frames.
void CSoldierObject::DrawMuzzleFlash(CDrawContext& ctx, CWeapon* weapon, bool mmg) {
    CDrawContext local(ctx);
    CStaticObject* flash = weapon->m_pMuzzleFlash;
    if (flash == NULL || (!(u8)weapon->GetMuzzleFlashStatus() && !mmg)) {
        return;
    }
    float spin = MathFunRandomReal(0.0f, 6.28f);
    if (((int)weapon->m_flashTime & 3) != 0 && !mmg) {
        return;
    }
    CMatrix tm;
    if (!mmg) {
        CMatrix bone;
        bone = m_bones[m_anim.m_pSkel->matrices[m_emitterPoint].joint];
        tm.Multiply(bone, m_tm);
        CMatrix attach;
        attach = m_attachTM[m_emitterPoint];
        tm.Multiply(attach, tm);
        CMatrix muzzle;
        muzzle = m_emitterTM;
        muzzle.RotateY(spin);
        tm.Multiply(muzzle, tm);
    } else {
        CStaticObject* gun = m_pMMG;
        StaticMesh* data = gun->m_pMesh->m_pData;
        int n = data->m_numHardPoints;
        StaticMeshHardPoint* hp = data->m_hardPoints;
        tm = gun->m_tm;
        for (int i = 0; i < n; i++, hp++) {
            if (hp->m_id == HARDPOINT_MUZZLE) {
                tm.Multiply(hp->m_tm, tm);
                break;
            }
        }
        tm.RotateY(spin);
    }
    flash->SetTMLocalToWorld(tm);
    flash->Draw(local);
}

// Drops everything held: the hand objects and all weapons.
static inline void DropWeapons(CSoldierObject* soldier) {
    soldier->DetachObject("lt_hand");
    soldier->DetachObject("rt_hand");
    for (int i = 0; i < SOLDIER_NUM_WEAPONS; i++) {
        if (soldier->m_pWeapons[i] != NULL) {
            if (g_scene.IsNodeInScene(soldier->m_pWeapons[i])) {
                g_scene.Remove(*soldier->m_pWeapons[i]);
            }
            g_WeaponFactory.DestroyWeapon(soldier->m_pWeapons[i]);
            soldier->m_pWeapons[i] = NULL;
        }
    }
    if (soldier->m_pMMGWeapon != NULL) {
        g_WeaponFactory.DestroyWeapon(soldier->m_pMMGWeapon);
        soldier->m_pMMGWeapon = NULL;
    }
}

void CSoldierObject::Draw(CDrawContext& ctx) {
    CDrawContext local(ctx);
    if (g_config.field_0x3) {
        return;
    }
    if (g_config.field_0x4) {
        return;
    }
    m_anim.BindTexture(m_pBodyTexture, m_bodyTexturePart);
    m_face.BindTexture(m_pFaceTexture, -1);
    if (m_fading && m_fadeTime > 0.5f * m_fadeDuration) {
        // Fade out over the second half.
        if (!m_fadeDropped && m_fadeTime > 0.75f * m_fadeDuration) {
            m_fadeDropped = 1;
            DropWeapons(this);
        }
        float half = 0.5f * m_fadeDuration;
        float t = (m_fadeTime - half) / half;
        if (t >= 1.0f) {
            t = 1.0f;
        }
        float alpha = 1.0f - t;
        m_anim.m_animFlags |= 0x100;
        s8 a = 128.0f * alpha;
        m_anim.field_0xA3C = a;
        m_face.m_animFlags |= 0x100;
        m_face.field_0xA3C = a;
        m_shadow.m_alphaScale = alpha;
    }
    if (g_cheats.field_0xF) {
        m_anim.m_animFlags |= 0x100;
        m_anim.field_0xA3C = 0;
        m_face.m_animFlags |= 0x100;
        m_face.field_0xA3C = 0;
    }
    CAnimObject::Draw(local);
    m_face.Draw(local);
    CharSkel_t* skel = m_anim.m_pSkel;
    CMatrix tm;
    for (u32 i = 0; i < skel->numMatrices; i++) {
        switch (m_attachType[i]) {
        case SOLDIER_ATTACH_NONE:
            break;
        case SOLDIER_ATTACH_OBJECT: {
            tm.Multiply(m_bones[skel->matrices[i].joint], m_tm);
            tm.Multiply(m_attachTM[i], tm);
            CStaticObject* obj = (CStaticObject*)m_attached[i];
            obj->SetTMLocalToWorld(tm);
            CStaticMesh* mesh = obj->m_pMesh;
            if ((u8)mesh->IsLightingEnabled()) {
                CStaticMesh::SetLightVolume(local.m_pLightVolume);
            }
            local.m_pWorld = &tm;
            if (obj->IsDrawEnabled()) {
                mesh->Draw(local, 0);
            }
            CWeapon* weapon = m_pWeapons[m_curWeapon];
            if (weapon != NULL && mesh == weapon->m_pMesh) {
                DrawMuzzleFlash(local, weapon, false);
            }
            break;
        }
        case SOLDIER_ATTACH_MESH: {
            tm.Multiply(m_bones[skel->matrices[i].joint], m_tm);
            tm.Multiply(m_attachTM[i], tm);
            local.m_pWorld = &tm;
            CStaticMesh* mesh = (CStaticMesh*)m_attached[i];
            if ((u8)mesh->IsLightingEnabled()) {
                CStaticMesh::SetLightVolume(local.m_pLightVolume);
            }
            mesh->Draw(local, 0);
            break;
        }
        }
    }
    if (m_floatingBoneOn && m_pFloatingObject != NULL) {
        CMatrix m;
        m = m_tm;
        m.PreTranslate(m_anim.m_frame.floatPos);
        m.RotateZ(-m_anim.m_frame.floatRot.z);
        m.RotateY(-m_anim.m_frame.floatRot.y);
        m.RotateX(-m_anim.m_frame.floatRot.x);
        m.Multiply(m_floatingTM, m);
        m_pFloatingObject->SetTMLocalToWorld(m);
        if (m_mmgFired) {
            DrawMuzzleFlash(local, m_pMMGWeapon, true);
        }
    }
    m_mmgFired = 0;
    m_pMMG = NULL;
    m_shadow.Draw(ctx);
}

int CStaticMesh::IsLightingEnabled() {
    return m_lighting;
}

// Head translation and tracking, then the face: queued face animation,
// mood, expression fades, blinking and the jaw.
void CSoldierObject::UpdateAnimation(float dt) {
    CAnimObject::UpdateAnimation(dt);
    if (m_headTrackPlayer || m_headTrackSteps > m_headTrackStep || m_headTrans != 0) {
        CharSkel_t* skel = m_anim.m_pSkel;
        u32 point = FindAttachPoint("VisionCone");
        if (point != (u32)-1) {
            if (m_headTrans != 0) {
                // Move the root so the vision cone joint stays where it is
                // (horizontally) while the animation moves the head.
                CVector3 root(*(CVector3*)m_bones[0].m[3]);
                CVector3 delta(*(CVector3*)m_bones[skel->matrices[point].joint].m[3]);
                delta.z = delta.z - root.z;
                delta.z = 0.0f;
                delta.x = delta.x - root.x;
                delta.y -= root.y;
                CVector3 prev(m_headTransOffset);
                CVector3 world;
                m_tm.TransformVector(world, delta);
                m_headTransOffset = world;
                switch (m_headTrans) {
                case 1: {
                    float inv = 1.0f / dt;
                    world.x = world.x * inv;
                    world.y *= inv;
                    world.z = world.z * inv;
                    m_velocity.x += world.x;
                    m_velocity.y += world.y;
                    m_velocity.z += world.z;
                    if (m_anim.m_pFrame->count != 0) {
                        CVector3 back;
                        back.x = -delta.x;
                        back.y = -delta.y;
                        back.z = -delta.z;
                        m_anim.AdjustRootTrans(back);
                    }
                    m_headTrans = 2;
                    break;
                }
                case 2: {
                    float inv = 1.0f / dt;
                    world.x = world.x - prev.x;
                    world.y = world.y - prev.y;
                    world.z = world.z - prev.z;
                    world.x = world.x * inv;
                    world.y = world.y * inv;
                    world.z = world.z * inv;
                    m_velocity.x += world.x;
                    m_velocity.y += world.y;
                    m_velocity.z += world.z;
                    if (m_anim.m_pFrame->count != 0) {
                        CVector3 back;
                        back.x = -delta.x;
                        back.y = -delta.y;
                        back.z = -delta.z;
                        m_anim.AdjustRootTrans(back);
                    }
                    break;
                }
                case 3: {
                    float inv = 1.0f / dt;
                    world.x = world.x * inv;
                    world.y = world.y * inv;
                    world.z = world.z * inv;
                    m_velocity.x += -world.x;
                    m_velocity.y += -world.y;
                    m_velocity.z += -world.z;
                    m_headTrans = 0;
                    break;
                }
                }
            }
            if (m_headTrackPlayer || m_headTrackSteps > m_headTrackStep) {
                // Turn the vision cone joint towards the player, blending in
                // (or back out) over m_headTrackSteps frames.
                CMatrix head;
                CMatrix parentInv;
                float t = (float)m_headTrackStep / (float)m_headTrackSteps;
                if (!m_headTrackPlayer) {
                    t = 1.0f - t;
                }
                int joint = skel->matrices[point].joint;
                u8 parent = skel->joints[joint].node->parent->index;
                head.Multiply(m_bones[joint], m_tm);
                CVector3 look;
                g_scene.GetPlayer(0)->m_camera.GetPosition(look);
                CVector3 headPos(*(CVector3*)head.m[3]);
                look.x = look.x - headPos.x;
                look.y = look.y - headPos.y;
                look.z = look.z - headPos.z;
                look.Normalize();
                parentInv.Multiply(m_bones[parent], m_tm);
                parentInv.SetPos(CVector3(0.0f, 0.0f, 0.0f));
                parentInv.Transpose();
                if (g_bConstraint) {
                    CMatrix body;
                    float scale = -1.0f;
                    body.Multiply(m_bones[4], m_tm);
                    body.SetPos(CVector3(0.0f, 0.0f, 0.0f));
                    // Keep it in front of the body...
                    CVector3 front(*(CVector3*)body.m[1]);
                    CVector3 flat(look);
                    flat.z = 0.0f;
                    front.z = 0.0f;
                    front.Normalize();
                    flat.Normalize();
                    float d = flat.z * front.z + (flat.x * front.x + flat.y * front.y);
                    if (d < 0.0f) {
                        float k = -2.0f * d;
                        scale = look.x * look.x + look.y * look.y;
                        flat.x = flat.x + k * front.x;
                        flat.y = flat.y + k * front.y;
                        flat.z = flat.z + k * front.z;
                    }
                    // ...and within 60 degrees of the body's horizontal plane.
                    CVector3 bodyUp(*(CVector3*)body.m[2]);
                    CVector3 vert(look);
                    vert.x = 0.0f;
                    float u = bodyUp.z * vert.z + (bodyUp.x * 0.0f + bodyUp.y * vert.y);
                    if (u > 0.8660254f) {
                        scale = 0.8660254f;
                        look.z = 0.5f;
                    } else if (u < -0.8660254f) {
                        scale = 0.8660254f;
                        look.z = -0.5f;
                    }
                    if (scale > -1.0f) {
                        look.x = scale * flat.x;
                        look.y = scale * flat.y;
                    }
                }
                CVector3 up(0.0f, 0.0f, 1.0f);
                CVector3 right;
                right.Cross(look, up);
                right.Normalize();
                up.Cross(right, look);
                up.Normalize();
                CMatrix m;
                m.Ident();
                m.SetRight(right);
                m.SetFront(look);
                m.SetUp(up);
                m.Multiply(m, parentInv);
                float ex;
                float ey;
                float ez;
                m.ToEulerXZY(ex, ey, ez);
                // Blend the joint's angles (tilted 20 degrees down) by t.
                s16* angles = m_anim.m_frame.jointData + joint * 3;
                s16 a0 = angles[0];
                s16 a1 = angles[1];
                s16 a2 = angles[2];
                s16 target0 = (int)(2670176.8f * (ex - 0.34906584f)) >> 8;
                s16 target1 = (int)(2670176.8f * ey) >> 8;
                s16 target2 = (int)(2670176.8f * ez) >> 8;
                angles[0] = (int)((float)(s16)(target0 - a0) * t) + a0;
                angles[1] = (int)((float)(s16)(target1 - a1) * t) + a1;
                angles[2] = (int)((float)(s16)(target2 - a2) * t) + a2;
                if (m_headTrackStep < m_headTrackSteps) {
                    m_headTrackStep++;
                }
            }
        }
    }

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

int CSoldierObject::SetFaceAnimationState(unsigned short state, long) {
    m_faceState = state;
    m_faceStateQueued = 1;
    return 0;
}

void CSoldierObject::RemoveWeapons() {
    DropWeapons(this);
}

// Frees the volumes, weapons, a helmet object and the AI.
void CSoldierObject::CleanUp() {
    if (m_pLocalVolume != NULL) {
        FreeAnimatedVolume(m_pLocalVolume);
    }
    if (m_pWorldVolume != NULL) {
        FreeAnimatedVolume(m_pWorldVolume);
    }
    if (m_pLocalBound != NULL) {
        FreeBoxVolume(m_pLocalBound);
    }
    if (m_pWorldBound != NULL) {
        FreeBoxVolume(m_pWorldBound);
    }
    DropWeapons(this);
    u32 point = FindAttachPoint("head");
    if (point != (u32)-1) {
        switch (m_attachType[point]) {
        case SOLDIER_ATTACH_OBJECT:
            CStaticObjectFactory::DeallocateStaticObject((CStaticObject*)m_attached[point]);
            break;
        }
    }
    m_aiDoodad.CleanUp();
}

CSoldierObject::~CSoldierObject() {}

void CSoldierObject::InitAI() {
    m_aiDoodad.Init(this);
}

int CSoldierObject::Init() {
    m_pFloatingObject = NULL;
    CAnimObject::Init();
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
    SetMood((EMoodType)0);
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
    m_faceStateQueued = 0;
    SetCollisionId((EClsnId)2);
    m_curWeapon = 0;
    memset(m_attached, 0, sizeof(m_attached));
    memset(m_attachType, 0, sizeof(m_attachType));
    memset(m_pWeapons, 0, sizeof(m_pWeapons));

    CAnimatedVolume* hit = AllocateAnimatedVolume();
    hit->SetRadius(1.2f);
    hit->SetCenter(CVector3(0.0f, 0.0f, 0.9f));
    hit->m_pOwner = this;
    m_pHitVolume = hit;
    m_pLocalVolume = hit;
    m_pWorldVolume = AllocateAnimatedVolume();
    m_localSphere1.SetRadius(0.25f);
    m_localSphere1.SetCenter(CVector3(0.0f, 0.0f, 0.55f));
    m_localSphere2.SetRadius(1.0f);
    m_localSphere2.SetCenter(CVector3(0.0f, 0.0f, 0.9f));
    CVolBox* box = AllocateBoxVolume();
    box->SetBasis(CVector3(1.0f, 0.0f, 0.0f), CVector3(0.0f, 1.0f, 0.0f), CVector3(0.0f, 0.0f, 1.0f));
    box->SetWidth(0.75f);
    box->SetHeight(1.6f);
    box->SetDepth(0.6f);
    box->SetCenter(CVector3(0.0f, 0.0f, 0.8f));
    m_pLocalBound = box;
    m_pWorldBound = AllocateBoxVolume();

    CVector3 right;
    CVector3 forward;
    CVector3 up;
    GetRightward(right);
    GetForward(forward);
    GetUpward(up);
    m_upperTM.SetRight(right);
    m_upperTM.SetFront(forward);
    m_upperTM.SetUp(up);
    CVector3 pos;
    GetPosition(pos);
    right.Scale(right, m_headPosition[0]);
    forward.Scale(forward, m_headPosition[1]);
    up.Scale(up, m_headPosition[2]);
    pos.Add(right);
    pos.Add(forward);
    pos.Add(up);
    m_upperTM.SetPos(pos);

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
    m_pMMGWeapon = g_WeaponFactory.CreateWeapon((EWeaponTypes)0x1A, this, 999);
    m_burstTimer = 0.0f;
    m_burstHits = 0;
    m_headTrans = 0;
    m_headTransOffset.z = 0.0f;
    m_headTransOffset.y = 0.0f;
    m_headTransOffset.x = 0.0f;
    END_RETURN_NOTHING
}

CSoldierObject::CSoldierObject() {}
