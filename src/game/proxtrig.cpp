// Proximity triggers: each frame, finds the property-BSP leaf each player,
// German, and moving box/tank is in, tests which of the leaf's triggers and
// light volumes contain it, and sends the triggers' enter/leave messages
// (and light volume enter/exit) for the changes since the last frame.
// Triggers have separate message lists for players and for other objects.
#include "game/proxtrig.h"
#include "game/BSGO_Basic.h"
#include "game/ISceneNode.h"
#include "game/IMovingSceneNode.h"
#include "game/bpdlight.h"
#include "game/bsmessage.h"
#include "game/bsobject.h"
#include "game/bstypes.h"
#include "game/matrix.h"
#include "game/objcreate.h"
#include "game/player.h"
#include "game/propdat.h"
#include "game/quaternion.h"
#include "game/scene.h"
#include "game/static_obj.h"
#include "game/vector.h"
#include "game/world_object.h"

// Trigger shapes (TriggerData::m_shape).
#define TRIGGER_SPHERE 0
#define TRIGGER_CYLINDER 1
#define TRIGGER_BOX 2

// Leaf list entries with this bit are light volumes.
#define PROX_LIGHT_VOLUME 0x8000

// Waypoints of this kind are ladders: players inside are climbing.
#define TRIGGER_TYPE_WAYPOINT 5
#define LADDER_CRC 0x2070A66D

// Non-trigger objects of this class load compartments when a player enters.
#define COMPARTMENT_LOADER_CRC 0x4D078F23

// A trigger's view of its property data.
struct TriggerData {
    /* 0x00 */ u32 m_classCRC;
    /* 0x04 */ u8 unk_0x4[0x8];
    /* 0x0C */ s32 m_type;  // 0: not a trigger
    /* 0x10 */ f32 m_position[3];
    /* 0x1C */ f32 m_orientation[4];
    /* 0x2C */ s16 m_shape;
    // 12.4 fixed point: sphere/cylinder radius or box width; cylinder top
    // and bottom (relative to the position) or box height and depth.
    /* 0x2E */ s16 m_size0;
    /* 0x30 */ s16 m_size1;
    /* 0x32 */ s16 m_size2;
    // Compartments a compartment loader acquires.
    /* 0x34 */ s16 m_compartments[4];
    /* 0x3C */ u8 unk_0x3C[0xC];
    /* 0x48 */ u32 m_kindCRC;
    /* 0x4C */ u8 unk_0x4C[0xC];
    // Message lists: [0] = count, then message ids.
    /* 0x58 */ PTR32(u32) m_playerEnter;
    /* 0x5C */ PTR32(u32) m_playerLeave;
    /* 0x60 */ PTR32(u32) m_objectEnter;
    /* 0x64 */ PTR32(u32) m_objectLeave;
    /* 0x68 */ PTR32(u32) m_playerActivate;
    /* 0x6C */ PTR32(u32) m_objectActivate;
};
FILE_SIZE_ASSERT(TriggerData, 0x70);

static int g_PopUpMsgTimer;

static inline const TriggerData* Data(const TriggerObject_struct* trigger) {
    return (const TriggerData*)trigger->m_data;
}

static inline float Fixed(s16 value) {
    return value * 0.0625f;
}

static inline bool IsLadder(const TriggerData* data) {
    return data->m_type == TRIGGER_TYPE_WAYPOINT && data->m_kindCRC == LADDER_CRC;
}

bool IsPointInTrigger(const CVector3& p, const TriggerObject_struct* trigger) {
    const TriggerData* data = Data(trigger);
    float x = data->m_position[0];
    float y = data->m_position[1];
    float z = data->m_position[2];
    switch (data->m_shape) {
    case TRIGGER_SPHERE: {
        float dy = p.y - y;
        float dx = p.x - x;
        float dz = p.z - z;
        float r = Fixed(data->m_size0);
        if (dz * dz + (dx * dx + dy * dy) > r * r) {
            return false;
        }
        return true;
    }
    case TRIGGER_CYLINDER: {
        float dy = p.y - y;
        float dx = p.x - x;
        float r = Fixed(data->m_size0);
        float dz = p.z - z;
        if (dx * dx + dy * dy > r * r) {
            return false;
        }
        if (dz <= Fixed(data->m_size1) && dz >= Fixed(data->m_size2)) {
            return true;
        }
        return false;
    }
    case TRIGGER_BOX: {
        CMatrix m;
        ((const CQuaternion*)Data(trigger)->m_orientation)->GetMatrix(m);
        const TriggerData* d = Data(trigger);
        CVector3 pos;
        pos.x = d->m_position[0];
        pos.y = d->m_position[1];
        pos.z = d->m_position[2];
        m.SetPos(pos);
        d = Data(trigger);
        // The point in the box's frame (rows of m are its axes).
        float ox = p.x - m.m[3][0];
        float oy = p.y - m.m[3][1];
        float oz = p.z - m.m[3][2];
        float halfWidth = Fixed(d->m_size0) * 0.5f;
        float a = m.m[0][0] * ox + m.m[0][1] * oy + m.m[0][2] * oz;
        float b = m.m[1][0] * ox + m.m[1][1] * oy + m.m[1][2] * oz;
        float c = m.m[2][0] * ox + m.m[2][1] * oy + m.m[2][2] * oz;
        float halfDepth = -Fixed(d->m_size2) * 0.5f;
        if (a > halfWidth || a < -halfWidth) {
            return false;
        }
        if (b > halfDepth || b < -halfDepth) {
            return false;
        }
        // (The original bounds the third axis above by the depth.)
        if (c <= halfDepth && c >= -(Fixed(d->m_size1) * 0.5f)) {
            return true;
        }
        return false;
    }
    default:
        return false;
    }
}

static inline void SendMessages(const u32* list, TriggerObject_struct* trigger, BSGO_Basic* go) {
    u32 count = 0;
    const u32* ids = NULL;
    if (list != NULL) {
        count = list[0];
        ids = list + 1;
    }
    for (u32 i = 0; i < count; i++, ids++) {
        BSSendMessage(*ids, trigger->m_object, trigger, go->m_object, NULL);
    }
}

// The list for players or for other objects (NULL when there is none).
static inline const u32* MessageList(bool object, u32* objectList, u32* playerList) {
    if (object) {
        return objectList;
    }
    return playerList;
}

// Sends the activate messages of the triggers `go` is in.
void TriggerActivateMessages(BSGO_Basic* go, bool object) {
    _ProximityData* prox = (_ProximityData*)go->GetProximityData();
    const _PropBSPLeaf* leaf = prox->m_newLeaf;
    u32 bit = 1;
    u32 mask = prox->m_newMask;
    const u16* end = leaf->m_triggers + leaf->m_numTriggers;
    for (const u16* id = leaf->m_triggers; id != end; id++, bit <<= 1) {
        if (!(mask & bit)) {
            continue;
        }
        u16 entry = *id;
        if (entry & PROX_LIGHT_VOLUME) {
            continue;
        }
        TriggerObject_struct* trigger = &g_pTriggerObjects[entry & 0x7FFF];
        const TriggerData* data = Data(trigger);
        if (data->m_type != 0) {
            SendMessages(MessageList(object, data->m_objectActivate, data->m_playerActivate), trigger, go);
        }
    }
}

static inline void EnterTrigger(BSGO_Basic* go, u16 entry, bool object) {
    int index = entry & 0x7FFF;
    if (entry & PROX_LIGHT_VOLUME) {
        IMovingSceneNode* node = go->GetSceneNode()->AsMovingNode();
        if (node != NULL) {
            node->EnterLightVolume(&g_pLightVolumes[index]);
        }
        return;
    }
    TriggerObject_struct* trigger = &g_pTriggerObjects[index];
    const TriggerData* data = Data(trigger);
    if (data->m_type != 0) {
        SendMessages(MessageList(object, data->m_objectEnter, data->m_playerEnter), trigger, go);
        if (IsLadder(Data(trigger)) && !object) {
            ((CPlayerObject*)go->GetSceneNode())->SetClimbingState(true);
        }
    } else if (data->m_classCRC == COMPARTMENT_LOADER_CRC && !object) {
        CWorldObject::SAcquireSet set = {0, 0, 0, 0};
        set.m_ids[0] = data->m_compartments[0];
        set.m_ids[1] = data->m_compartments[1];
        set.m_ids[2] = data->m_compartments[2];
        set.m_ids[3] = data->m_compartments[3];
        g_scene.m_world->Acquire(set, true);
    }
}

static inline void LeaveTrigger(BSGO_Basic* go, u16 entry, bool object) {
    int index = entry & 0x7FFF;
    if (entry & PROX_LIGHT_VOLUME) {
        IMovingSceneNode* node = go->GetSceneNode()->AsMovingNode();
        if (node != NULL) {
            node->ExitLightVolume(&g_pLightVolumes[index]);
        }
        return;
    }
    TriggerObject_struct* trigger = &g_pTriggerObjects[index];
    const TriggerData* data = Data(trigger);
    if (data->m_type != 0) {
        SendMessages(MessageList(object, data->m_objectLeave, data->m_playerLeave), trigger, go);
        if (IsLadder(Data(trigger)) && !object) {
            ((CPlayerObject*)go->GetSceneNode())->SetClimbingState(false);
        }
    }
}

// Sends enter/leave messages for the changes between the last two checks.
// The leaf lists are sorted, so a change of leaf is a merge of the two.
void TriggerEnterAndLeaveMessages(BSGO_Basic* go, bool object) {
    _ProximityData* prox = (_ProximityData*)go->GetProximityData();
    const _PropBSPLeaf* newLeaf = prox->m_newLeaf;
    const _PropBSPLeaf* oldLeaf = prox->m_oldLeaf;
    if (newLeaf == oldLeaf || oldLeaf == NULL) {
        u32 changed = prox->m_newMask ^ prox->m_oldMask;
        u32 entered = changed & prox->m_newMask;
        u32 left = changed & prox->m_oldMask;
        int i;
        for (i = 0; entered != 0; entered >>= 1, i++) {
            if (entered & 1) {
                EnterTrigger(go, prox->m_newLeaf->m_triggers[i], object);
            }
        }
        for (i = 0; left != 0; left >>= 1, i++) {
            if (left & 1) {
                LeaveTrigger(go, prox->m_newLeaf->m_triggers[i], object);
            }
        }
        return;
    }

    const u16* n = newLeaf->m_triggers;
    const u16* o = oldLeaf->m_triggers;
    u32 newMask = prox->m_newMask;
    u32 oldMask = prox->m_oldMask;
    const u16* newEnd = n + newLeaf->m_numTriggers;
    const u16* oldEnd = o + oldLeaf->m_numTriggers;
    u32 newBit = 1;
    u32 oldBit = 1;
    while (n != newEnd && o != oldEnd) {
        u16 a = *n;
        u16 b = *o;
        if (a < b) {
            if (newBit & newMask) {
                EnterTrigger(go, a, object);
            }
            newBit <<= 1;
            n++;
        } else if (a > b) {
            if (oldBit & oldMask) {
                LeaveTrigger(go, b, object);
            }
            oldBit <<= 1;
            o++;
        } else {
            u32 inNew = newBit & newMask;
            u32 inOld = oldBit & oldMask;
            if (inNew != 0 && inOld == 0) {
                EnterTrigger(go, a, object);
            } else if (inOld != 0 && inNew == 0) {
                LeaveTrigger(go, b, object);
            }
            newBit <<= 1;
            oldBit <<= 1;
            n++;
            o++;
        }
    }
    for (; n != newEnd; n++, newBit <<= 1) {
        if (newMask & newBit) {
            EnterTrigger(go, *n, object);
        }
    }
    for (; o != oldEnd; o++, oldBit <<= 1) {
        if (oldMask & oldBit) {
            LeaveTrigger(go, *o, object);
        }
    }
}

static inline bool InLightVolume(const BPDLightVolume* volume, const CVector3& p) {
    const BPDPlane* plane = volume->planes;
    for (int i = volume->numPlanes; i > 0; i--, plane++) {
        if (plane->d + (plane->n[2] * p.z + (plane->n[0] * p.x + plane->n[1] * p.y)) < 0.0f) {
            return false;
        }
    }
    return true;
}

// Records which of `leaf`'s triggers and light volumes contain `p`. Only
// triggers with messages for this kind of object count; `force` (an object
// being moved by a script) records light volumes only.
void CheckLeafsTriggers(const _PropBSPLeaf* leaf, const CVector3& p, _ProximityData* prox, bool object,
                        bool force) {
    u32 bit = 1;
    u32 mask = 0;
    for (u32 i = 0; i < leaf->m_numTriggers; i++, bit <<= 1) {
        u16 entry = leaf->m_triggers[i];
        int index = entry & 0x7FFF;
        if (entry & PROX_LIGHT_VOLUME) {
            if (InLightVolume(&g_pLightVolumes[index], p)) {
                mask |= bit;
            }
        } else if (object && !force) {
            TriggerObject_struct* trigger = &g_pTriggerObjects[index];
            const TriggerData* data = Data(trigger);
            if (data->m_type != 0 &&
                (data->m_objectEnter != NULL || data->m_objectLeave != NULL || data->m_objectActivate != NULL) &&
                IsPointInTrigger(p, trigger)) {
                mask |= bit;
            }
        } else if (!force) {
            TriggerObject_struct* trigger = &g_pTriggerObjects[index];
            const TriggerData* data = Data(trigger);
            int type = data->m_type;
            if (type == 0 || data->m_playerEnter != NULL || data->m_playerLeave != NULL ||
                data->m_playerActivate != NULL) {
                if (IsPointInTrigger(p, trigger)) {
                    mask |= bit;
                }
            } else if (type == TRIGGER_TYPE_WAYPOINT && data->m_kindCRC == LADDER_CRC && IsPointInTrigger(p, trigger)) {
                mask |= bit;
            }
        }
    }
    prox->m_oldMask = prox->m_newMask;
    prox->m_newMask = mask;
    prox->m_oldLeaf = prox->m_newLeaf;
    prox->m_newLeaf = leaf;
}

// Checks `go` against the leaf it is in (its last one if it is still in
// it, else found from the BSP root) and sends the resulting messages.
static inline void UpdateObject(BSGO_Basic* go, bool object, bool force) {
    _ProximityData* prox = (_ProximityData*)go->GetProximityData();
    if (prox == NULL || (go->m_object != NULL && go->m_object->m_uniqueID == -1)) {
        return;
    }
    CVector3 pos;
    go->GetSceneNode()->GetPosition(pos);
    const _PropBSPLeaf* leaf = prox->m_newLeaf;
    bool inLeaf;
    if (leaf != NULL && pos.x <= leaf->m_maxX && pos.x >= leaf->m_minX && pos.y <= leaf->m_maxY &&
        pos.y >= leaf->m_minY) {
        inLeaf = true;
    } else {
        inLeaf = false;
    }
    if (inLeaf) {
        CheckLeafsTriggers(leaf, pos, prox, object, force);
        TriggerEnterAndLeaveMessages(go, object);
        return;
    }
    bool splitY = true;
    _PropBSPNode* node = g_pPBSPHead;
    for (;;) {
        bool above;
        if (splitY) {
            above = pos.y > node->m_split;
        } else {
            above = pos.x > node->m_split;
        }
        int side = above ? 1 : 0;
        if (node->m_isLeaf[side]) {
            CheckLeafsTriggers((const _PropBSPLeaf*)node->m_children[side], pos, prox, object, force);
            break;
        }
        node = (_PropBSPNode*)node->m_children[side];
        splitY = !splitY;
    }
    TriggerEnterAndLeaveMessages(go, object);
}

// Updates an object a script just moved.
void ForceUpdateProximityTriggerStatus(BSGO_Basic* go, bool force) {
    UpdateObject(go, true, force);
}

// Boxes and tanks only count while moving under their own power.
static inline bool MovesItself(CStaticObject* obj) {
    bool moving = false;
    if (obj->m_translating || obj->m_rotating || obj->m_motionPlaying) {
        moving = true;
    }
    return moving;
}

void UpdateProximityTriggerStatus() {
    if (g_PopUpMsgTimer > 0) {
        g_PopUpMsgTimer--;
    }
    for (int i = 0; i < g_scene.m_numPlayers; i++) {
        CPlayerObject* player = g_scene.GetPlayer(i);
        if (player != NULL && player->m_triggerChecks) {
            UpdateObject(&player->m_bsgo, false, false);
        }
    }
    BSGO_Basic* go;
    for (go = g_pGermanList; go != NULL; go = go->m_pNext) {
        UpdateObject(go, true, false);
    }
    for (go = g_pBoxList; go != NULL; go = go->m_pNext) {
        CStaticObject* obj = go->GetSceneNode()->AsStaticObject();
        if (obj != NULL && MovesItself(obj)) {
            UpdateObject(go, true, false);
        }
    }
    for (go = g_pTankList; go != NULL; go = go->m_pNext) {
        CStaticObject* obj = (CStaticObject*)go->GetSceneNode()->AsHierObject();
        if (obj != NULL && MovesItself(obj)) {
            UpdateObject(go, true, false);
        }
    }
}
