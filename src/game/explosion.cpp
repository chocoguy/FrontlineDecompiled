// CExplosion: blasts from grenades, shells and scripts.
#include "game/explosion.h"
#include "game/ISceneNode.h"
#include "game/IMovingSceneNode.h"
#include "game/IVolume.h"
#include "game/player.h"
#include "game/Soldier_object.h"
#include "game/scene.h"
#include "game/world_object.h"
#include "game/collision.h"
#include "game/line.h"
#include "game/matrix.h"
#include "game/bullet.h"
#include "game/thrownbullet.h"
#include "game/bstypes.h"
#include "game/bsevents.h"
#include "game/bsobject.h"
#include "game/bsbifunc.h"
#include "game/Moh2.h"
#include "game/input.h"
#include "game/ShellMenu.h"
#include "stdlib.h"

// Does the world block the line from `from` to `to`?
static inline bool LineBlocked(const CVector3& from, const CVector3& to) {
    CLine3 line;
    line.SetSE(from, to);
    CCollision clsn;
    clsn.m_flags &= ~CLSN_FLAG_NEAREST;
    const IVolume* world = g_scene.m_world->GetWorldBoundingVolume((ISceneNode::EVolumeType)0);
    int result = world->TestCollision(line, clsn, false);
    return result != CLSN_NONE;
}

// Three lines from the blast to the node, in the node's frame (facing away
// from the blast): low and to the side, middle and to the other side, and
// near the ground. The node is shielded only when all three hit the world.
// Blasts from below (bullet types 0x15..0x17 more than 2 units under the
// node) aim the lines' blast end downward.
bool CExplosion::CheckForObstructions(ISceneNode& node) {
    CVector3 center = m_sphere.GetCenter();
    CVector3 pos;
    node.GetPosition(pos);
    CVector3 dir;
    dir.x = pos.x - center.x;
    dir.y = pos.y - center.y;
    dir.z = pos.z - center.z;
    dir.z = 0.0f;
    float len = dir.Normalize();
    if (len < 0.001f) {
        return false;
    }
    CVector3 up(0.0f, 0.0f, 1.0f);
    CVector3 right;
    right.Cross(dir, up);
    CMatrix frame;
    frame.SetPos(CVector3(0.0f, 0.0f, 0.0f));
    frame.SetRight(right);
    frame.SetFront(dir);
    CVector3 frameUp;
    frameUp.Cross(right, dir);
    frame.SetUp(frameUp);

    float height = 1.0f;
    if (node.AsPlayerObject() != NULL) {
        if (node.AsPlayerObject()->IsPlayerCrouching()) {
            height = 0.33f;
        }
        pos.z -= node.AsPlayerObject()->m_localCapsule.m_radius;
    } else if (node.AsSoldierObject() != NULL) {
        CMatrix look;
        node.AsSoldierObject()->GetLookMatrix(look);
        float eye = look.m[3][2] - pos.z;
        if (eye < 0.0f) {
            eye = 0.0f;
        }
        height = eye / 1.6f;
    }

    bool below;
    if (m_bulletType >= 0x15 && m_bulletType < 0x18) {
        CVector3 nodePos;
        node.GetPosition(nodePos);
        CVector3 c = m_sphere.GetCenter();
        below = c.z - nodePos.z > 2.0f;
    } else {
        below = false;
    }
    int sign = 1;
    if (below) {
        sign = -1;
    }

    float top = 1.6f * height;
    static const float sides[3] = {0.25f, -0.25f, 0.0f};
    static const float heights[3] = {1.0f, 0.5f, 0.01f};
    for (int i = 0; i < 3; i++) {
        float h = heights[i] * top;
        CVector3 nodeOffset(sides[i], 0.0f, h);
        CVector3 blastOffset(0.0f, 0.0f, (float)sign * h);
        CVector3 blast = m_sphere.GetCenter();
        CVector3 a;
        CVector3 b;
        frame.TransformPoint(a, nodeOffset);
        frame.TransformPoint(b, blastOffset);
        a.x += pos.x;
        a.y += pos.y;
        a.z += pos.z;
        b.x += blast.x;
        b.y += blast.y;
        b.z += blast.z;
        if (!LineBlocked(b, a)) {
            return false;
        }
    }
    return true;
}

// Script events 0xB0 (explosion; falls back to 0x2B, hit, when the script
// doesn't handle it) with damage falling off from the center to half at
// the edge, pushes nodes within 80% of the radius, HUD marker and rumble
// for players. Unscripted thrown bullets (grenades) go off too.
bool CExplosion::Visit(ISceneNode& node) {
    if (node.GetScriptObject() == NULL) {
        CBullet* bullet = node.AsBullet();
        if (bullet != NULL) {
            CThrownBullet* thrown = bullet->AsThrown();
            if (thrown != NULL) {
                if (CheckForObstructions(node)) {
                    return false;
                }
                thrown->Explode();
            }
        }
        return false;
    }
    if (CheckForObstructions(node)) {
        return false;
    }
    float pushRange = 0.8f * m_sphere.GetRadius();
    BS_STRUCT_HitByBulletInfo_struct* event =
        (BS_STRUCT_HitByBulletInfo_struct*)BSEventGetEventMemoryBlock(sizeof(BS_STRUCT_HitByBulletInfo_struct));
    CVector3 forward;
    CVector3 dir;
    node.GetForward(forward);
    node.GetPosition(dir);
    CVector3 center = m_sphere.GetCenter();
    dir.x -= center.x;
    dir.y -= center.y;
    dir.z -= center.z;
    float dist = dir.Length();
    float scale = 1.0f - dist / m_sphere.GetRadius();
    if (scale < 0.5f) {
        scale = 0.5f;
    }
    event->m_damage = (int)(m_damage * scale);
    event->m_bulletType = m_bulletType;
    if (m_pSource != NULL) {
        event->m_firedBy = m_pSource->GetScriptObject();
    } else {
        event->m_firedBy = NULL;
    }
    if (forward.x * dir.x + forward.y * dir.y + forward.z * dir.z <= 0.0f) {
        event->m_side = 0;
    } else {
        event->m_side = 1;
    }
    if (node.AsPlayerObject() != NULL) {
        if (g_cheats.m_invincible) {
            event->m_damage = 0;
        } else if (g_cheats.field_0xE) {
            event->m_damage = 1000;
        }
        if (g_bInMultiplayerMode && event->m_firedBy != NULL) {
            if (event->m_side == 1) {
                event->m_location = 10;
            } else {
                event->m_location = 9;
            }
            node.AsPlayerObject()->SetMPShotLocation(event);
        }
    }

    if (node.AsMovingNode() != NULL && dir.Length() < pushRange) {
        bool attached = false;
        CPlayerObject* player = node.AsPlayerObject();
        if (player != NULL) {
            attached = player->m_attached;
        }
        CSoldierObject* soldier = node.AsSoldierObject();
        if ((soldier == NULL || !soldier->field_0x3161) && !attached) {
            IMovingSceneNode* moving = node.AsMovingNode();
            dir.Normalize();
            if (player != NULL && m_fromScript) {
                dir.z = g_fScriptExplosionHeightScaleOnPlayer * (0.5f * scale + 0.5f);
                moving->ApplyForceTo(dir, g_fScriptExplosionVelocityScaleOnPlayer * (30.0f * scale + 20.0f));
            } else {
                dir.z = 0.5f * scale + 0.5f;
                moving->ApplyForceTo(dir, 30.0f * scale + 20.0f);
            }
        }
        if (!BSObjectTriggerEvent(node.GetScriptObject(), 0xB0, event, NULL, false)) {
            BS_STRUCT_HitByBulletInfo_struct* hit = (BS_STRUCT_HitByBulletInfo_struct*)BSEventGetEventMemoryBlock(
                sizeof(BS_STRUCT_HitByBulletInfo_struct));
            if (hit != NULL) {
                *hit = *event;
                if (!g_bInMultiplayerMode) {
                    hit->m_bulletType = 5;
                }
                if (event->m_side == 1) {
                    hit->m_location = 10;
                } else {
                    hit->m_location = 9;
                }
                BSObjectTriggerEvent(node.GetScriptObject(), 0x2B, hit, NULL, false);
            }
        }
    } else if (node.AsHierObject() != NULL) {
        event->m_location = 0;
        BSObjectTriggerEvent(node.GetScriptObject(), 0xB0, event, NULL, false);
    } else if (node.AsCollisionVolume() != NULL) {
        BSObjectTriggerEvent(node.GetScriptObject(), 0x2B, event, NULL, false);
    } else {
        if (!g_bInMultiplayerMode) {
            event->m_bulletType = 5;
        }
        event->m_location = rand() % 21;
        BSObjectTriggerEvent(node.GetScriptObject(), 0x2B, event, NULL, false);
    }

    CPlayerObject* player = node.AsPlayerObject();
    if (player != NULL) {
        CVector3 playerForward;
        CVector3 right;
        player->GetForward(playerForward);
        player->GetRightward(right);
        float alongRight = right.x * dir.x + right.y * dir.y + right.z * dir.z;
        float alongForward = playerForward.x * dir.x + playerForward.y * dir.y + playerForward.z * dir.z;
        if (alongRight < -0.7071f) {
            player->m_ui.AddHitDirection(2);
        } else if (alongRight > 0.7071f) {
            player->m_ui.AddHitDirection(4);
        } else if (alongForward < 0.0f) {
            player->m_ui.AddHitDirection(1);
        } else {
            player->m_ui.AddHitDirection(3);
        }
        g_inputMgr.SetFeedback(player->m_controllerPort, 2, 0, scale, 1.0f);
    }
    return false;
}

CExplosion::~CExplosion() {}

CExplosion::CExplosion(CScene& scene, CVector3 center, float radius, float loudness, float damage,
                       EScriptBulletType type, ISceneNode* source, bool fromScript)
    : m_sphere(center, radius) {
    m_loudness = loudness;
    m_damage = damage;
    m_bulletType = type;
    m_pSource = source;
    m_fromScript = fromScript;
    MakeNoise(NULL, NULL, center, 0x34, 6.0 * loudness, true, 5);
    scene.VisitAllNodesInVolume(*this, m_sphere, (ISceneNode::EVolumeType)0, true);
    if (scene.m_world != NULL) {
        scene.m_world->HandleExplosion(*this);
    }
}
