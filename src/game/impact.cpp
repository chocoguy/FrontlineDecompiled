// CImpact: scripted melee-style hits (BIFunc_CreateImpact).
#include "game/impact.h"
#include "game/ISceneNode.h"
#include "game/IVolume.h"
#include "game/player.h"
#include "game/Soldier_object.h"
#include "game/scene.h"
#include "game/world_object.h"
#include "game/collision.h"
#include "game/SClsnContact.h"
#include "game/line.h"
#include "game/fiber.h"
#include "game/surfacetype.h"
#include "game/bstypes.h"
#include "game/bsevents.h"
#include "game/bsobject.h"
#include "game/ShellMenu.h"
#include "game/Moh2.h"
#include "game/input.h"
#include "game/sound.h"

// The team of a scripted node (0xFF when it has none).
static inline u8 ScriptTeam(ISceneNode* node) {
    BSObject* obj = node->GetScriptObject();
    if (obj != NULL && obj->m_trigger != NULL) {
        return obj->m_trigger->m_data->m_team;
    }
    return 0xFF;
}

// Keeps the nearest node the source faces (soldiers and players within
// ~32 degrees of its aim; anything else when its line of reach, 1.275 units
// long, touches the node) with nothing solid in between.
bool CImpact::Visit(ISceneNode& node) {
    if (&node == m_pSource) {
        return false;
    }
    if (node.GetCollisionId() == -1) {
        return false;
    }
    if (node.GetScriptObject() == NULL) {
        return false;
    }
    if (node.AsSoldierObject() != NULL && m_pSource->AsSoldierObject() != NULL) {
        u8 nodeTeam = ScriptTeam(&node);
        u8 sourceTeam = ScriptTeam(m_pSource);
        // Soldiers don't hit their own side, and nobody hits neutrals.
        if (nodeTeam != 0xFF && sourceTeam != 0xFF && (sourceTeam == nodeTeam || nodeTeam == 0)) {
            return false;
        }
    }

    CVector3 eye;
    CVector3 aim;
    if (m_pSource->AsPlayerObject() != NULL) {
        u8 team = node.GetScriptObject()->m_trigger->m_data->m_team;
        if (node.AsSoldierObject() != NULL && team == 0) {
            return false;
        }
        if (node.AsPlayerObject() != NULL && g_Shell.field_0x1434 == 0 &&
            m_pSource->AsPlayerObject()->m_team == node.AsPlayerObject()->m_team) {
            return false;
        }
        CCamera& camera = m_pSource->AsPlayerObject()->m_camera;
        camera.GetForward(aim);
        camera.GetPosition(eye);
    } else {
        m_pSource->GetForward(aim);
        m_pSource->GetPosition(eye);
        eye.x += 0.0f;
        eye.y += 0.0f;
        eye.z += 0.75f;
    }

    CVector3 hit;
    CVector3 delta;
    if (node.AsSoldierObject() != NULL || node.AsPlayerObject() != NULL) {
        if (g_bInMultiplayerMode) {
            if (node.AsPlayerObject() == NULL) {
                return false;
            }
            node.AsPlayerObject()->m_camera.GetPosition(hit);
            hit.x -= 0.0f;
            hit.y -= 0.0f;
            hit.z -= 0.5f;
        } else {
            node.GetPosition(hit);
            hit.x += 0.0f;
            hit.y += 0.0f;
            hit.z += 0.75f;
        }
        delta.x = hit.x - eye.x;
        delta.y = hit.y - eye.y;
        delta.z = hit.z - eye.z;
        CVector3 dir(delta);
        dir.Normalize();
        if (dir.x * aim.x + dir.y * aim.y + dir.z * aim.z < 0.85f) {
            return false;
        }
    } else {
        CVector3 reach(aim);
        reach.Normalize();
        reach.x *= 1.275f;
        reach.y *= 1.275f;
        reach.z *= 1.275f;
        CVector3 start(eye);
        reach.z += start.z;
        reach.x += start.x;
        reach.y += start.y;
        CVolFiber fiber(start, reach);
        const IVolume* volume = node.GetWorldBoundingVolume((ISceneNode::EVolumeType)0);
        CCollision clsn;
        if (volume->TestCollision(fiber, clsn, false) != CLSN_TOUCHING) {
            return false;
        }
        CVector3 at;
        at.y = start.y + (reach.y - start.y) * clsn.m_fraction;
        at.x = start.x + (reach.x - start.x) * clsn.m_fraction;
        at.z = start.z + (reach.z - start.z) * clsn.m_fraction;
        hit = at;
        delta.x = hit.x - eye.x;
        delta.y = hit.y - eye.y;
        delta.z = hit.z - eye.z;
    }

    // Walls in between block the hit unless they can be shot through.
    CLine3 line;
    line.SetSE(eye, hit);
    CVolFiber sight(line);
    const IVolume* world = g_scene.m_world->GetWorldBoundingVolume((ISceneNode::EVolumeType)0);
    CCollision clsn;
    if (world->TestCollision(sight, clsn, true) == CLSN_TOUCHING) {
        int surface = clsn.m_pContacts->m_pPoly->m_surfaceType;
        if ((s16)surface >= 0 && !CSurfaceType::IsShootThru((s16)surface)) {
            return false;
        }
    }
    float dist = delta.Length();
    if (dist < m_closestDist) {
        m_closestDist = delta.Length();
        m_pClosest = &node;
        m_hitPos = hit;
    }
    return false;
}

// Script event 0x2B (hit) on the nearest node: double damage from behind,
// the player's hit marker and rumble, and a hit sound for characters.
int CImpact::DamageClosestObject() {
    BS_STRUCT_HitByBulletInfo_struct* event =
        (BS_STRUCT_HitByBulletInfo_struct*)BSEventGetEventMemoryBlock(sizeof(BS_STRUCT_HitByBulletInfo_struct));
    CVector3 forward;
    CVector3 dir;
    m_pClosest->GetForward(forward);
    m_pClosest->GetPosition(dir);
    CVector3 center = m_volume.GetCenter();
    dir.x -= center.x;
    dir.y -= center.y;
    dir.z -= center.z;
    event->m_bulletType = 0x19;
    if (m_pSource != NULL) {
        event->m_firedBy = m_pSource->GetScriptObject();
    } else {
        event->m_firedBy = NULL;
    }
    if (forward.x * dir.x + forward.y * dir.y + forward.z * dir.z <= 0.0f) {
        event->m_damage = (int)field_0x34;
        event->m_side = 0;
    } else {
        event->m_damage = (int)(2.0f * field_0x34);
        event->m_side = 1;
    }
    if (g_cheats.m_invincible && m_pClosest->AsPlayerObject() != NULL) {
        event->m_damage = 0;
    }
    if (m_pClosest->AsPlayerObject() != NULL) {
        CPlayerObject* player = m_pClosest->AsPlayerObject();
        CVector3 right;
        m_pClosest->GetRightward(right);
        float alongRight = right.x * dir.x + right.y * dir.y + right.z * dir.z;
        float alongForward = forward.x * dir.x + forward.y * dir.y + forward.z * dir.z;
        if (alongRight < -0.7071f) {
            player->m_ui.AddHitDirection(2);
        } else if (alongRight > 0.7071f) {
            player->m_ui.AddHitDirection(4);
        } else if (alongForward < 0.0f) {
            player->m_ui.AddHitDirection(1);
        } else {
            player->m_ui.AddHitDirection(3);
        }
    } else if (event->m_side == 1) {
        event->m_location = 0;
    } else {
        event->m_location = 1;
    }
    if (m_pClosest->AsPlayerObject() != NULL) {
        g_inputMgr.SetFeedback(m_pClosest->AsPlayerObject()->m_controllerPort, 0xC, 0, 1.0f, 0.1f);
    }
    if (m_pSource->AsPlayerObject() != NULL) {
        g_inputMgr.SetFeedback(m_pSource->AsPlayerObject()->m_controllerPort, 0xC, 0, 1.0f, 0.1f);
    }
    if (m_pClosest->AsPlayerObject() != NULL || m_pClosest->AsSoldierObject() != NULL) {
        int sound[5];
        sound[0] = 3;
        sound[1] = 3;
        sound[2] = 0;
        sound[3] = (u16)GetAzimuth(m_hitPos, g_scene.GetPlayer(0));
        sound[4] = 1;
        AEMS_BeginEvent(sound);
    }
    BSObjectTriggerEvent(m_pClosest->GetScriptObject(), 0x2B, event, NULL, false);
    return 1;
}

CImpact::~CImpact() {}
