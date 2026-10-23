// Attachable objects: static objects whose mesh hard points carry other
// nodes (props, soldiers, the player).
#include "game/attachobject.h"

#include "game/staticmesh.h"
#include "game/anim_obj.h"
#include "game/player.h"
#include "game/scene.h"

// The anim object's root bone matrix (CAnimObject + 0xF40, not typed yet).
#define ANIM_ROOT_BONE(anim) (*(const CMatrix*)(anim)->m_bones)


// Copies hard point `id`'s matrix to its slot; returns the slot (0 when the
// mesh has no such point).
static inline int FindHardPoint(CAttachableObject* obj, int id) {
    int slot = 0;
    StaticMesh* data = obj->m_pMesh->m_pData;
    StaticMeshHardPoint* point = data->m_hardPoints;
    for (int i = 0; i < data->m_numHardPoints; i++, point++) {
        if (point->m_id == id) {
            slot = i;
            obj->m_offsets[i] = point->m_tm;
            break;
        }
    }
    return slot;
}

// Moves the node in `slot` to its hard point, or drops it if it has been
// destroyed since it was attached.
static inline void MoveAttached(CAttachableObject* obj, int slot) {
    if (obj->m_attachedId[slot] != obj->m_attached[slot]->GetScriptObject()->m_uniqueID) {
        obj->m_attached[slot] = NULL;
        return;
    }
    CMatrix tm;
    tm = obj->m_tm;
    tm.Multiply(obj->m_offsets[slot], tm);
    switch (obj->m_attachType[slot]) {
    case CAttachableObject::ATTACH_STATIC:
        obj->m_attached[slot]->AsStaticObject()->SetTMLocalToWorld(tm);
        break;
    case CAttachableObject::ATTACH_ANIM: {
        CAnimObject* anim = obj->m_attached[slot]->AsAnimObject();
        if (anim->field_0x24B4) {
            CMatrix inv;
            inv.Inverse(ANIM_ROOT_BONE(anim));
            tm.Multiply(inv, tm);
        }
        ((IMovingSceneNode*)anim)->SetTMLocalToWorld(tm);
        break;
    }
    case CAttachableObject::ATTACH_PLAYER:
        if (obj->m_attached[slot]->AsPlayerObject()->m_attachedKeepWeapon) {
            CVector3 pos = *(const CVector3*)tm.m[3];
            obj->m_attached[slot]->AsPlayerObject()->field_0x740.Copy(pos);
        } else {
            ((IMovingSceneNode*)obj->m_attached[slot]->AsPlayerObject())->SetTMLocalToWorld(tm);
        }
        break;
    }
}

CAttachableObject::CAttachableObject() {
    for (int i = 0; i < MAX_ATTACHED; i++) {
        m_attached[i] = NULL;
    }
}

CAttachableObject::CAttachableObject(CStaticMesh* mesh, int numAnimFrames, int animFrameTime,
                                     int numMotionFrames, int motionFrameTime, int motionRelative,
                                     CStaticObject::MotionFrame* motion)
    : CStaticObject(mesh, numAnimFrames, animFrameTime, numMotionFrames, motionFrameTime,
                    motionRelative, motion) {
    for (int i = 0; i < MAX_ATTACHED; i++) {
        m_attached[i] = NULL;
    }
}

int CAttachableObject::AttachObject(CStaticObject* node, int hardPoint) {
    if (node == NULL) {
        return 0;
    }
    int slot = FindHardPoint(this, hardPoint);
    m_attached[slot] = node;
    m_attachedId[slot] = node->GetScriptObject()->m_uniqueID;
    m_attachType[slot] = ATTACH_STATIC;
    g_scene.ExcludeCollision(*this, *node, -1);
    return 1;
}

// With `hideWeapon` the player is fully carried (and can't shoot); without
// it only the position follows.
int CAttachableObject::AttachObject(CPlayerObject* player, int hardPoint, bool hideWeapon) {
    if (player == NULL) {
        return 0;
    }
    int slot = FindHardPoint(this, hardPoint);
    m_attached[slot] = (ISceneNode*)player;
    m_attachedId[slot] = ((ISceneNode*)player)->GetScriptObject()->m_uniqueID;
    m_attachType[slot] = ATTACH_PLAYER;
    player->m_attached = 1;
    player->m_attachedKeepWeapon = !hideWeapon;
    player->SetHideWeapon(hideWeapon);
    g_scene.ExcludeCollision(*this, *(ISceneNode*)player, -1);
    MoveAttached(this, slot);
    return 1;
}

int CAttachableObject::AttachObject(CAnimObject* anim, int hardPoint) {
    if (anim == NULL) {
        return 0;
    }
    int slot = FindHardPoint(this, hardPoint);
    m_attached[slot] = (ISceneNode*)anim;
    m_attachedId[slot] = ((ISceneNode*)anim)->GetScriptObject()->m_uniqueID;
    m_attachType[slot] = ATTACH_ANIM;
    anim->SetAttached(this != NULL);
    g_scene.ExcludeCollision(*this, *(ISceneNode*)anim, -1);
    return 1;
}

int CAttachableObject::DetachObject(ISceneNode* node) {
    int found = 0;
    for (int i = 0; i < MAX_ATTACHED; i++) {
        if (m_attached[i] == node) {
            found = 1;
            if (m_attachType[i] == ATTACH_ANIM) {
                m_attached[i]->AsAnimObject()->SetAttached(false);
            } else if (m_attachType[i] == ATTACH_PLAYER) {
                CPlayerObject* player = m_attached[i]->AsPlayerObject();
                player->m_attached = 0;
                player = m_attached[i]->AsPlayerObject();
                player->m_attachedKeepWeapon = 1;
                m_attached[i]->AsPlayerObject()->SetHideWeapon(false);
            }
            m_attached[i] = NULL;
            m_attachType[i] = ATTACH_NONE;
        }
    }
    return found;
}

void CAttachableObject::Draw(CDrawContext& ctx) {
    CStaticObject::Draw(ctx);
}

int CAttachableObject::IsVisible(CDrawContext& ctx) const {
    return CStaticObject::IsVisible(ctx);
}

void CAttachableObject::UpdateAttachedObjects() {
    for (int i = 0; i < MAX_ATTACHED; i++) {
        if (m_attached[i] != NULL) {
            MoveAttached(this, i);
        }
    }
}

int CAttachableObject::BeginUpdate(float dt) {
    return CStaticObject::BeginUpdate(dt);
}

void CAttachableObject::UpdateAI(float) {}

void CAttachableObject::CommitAI() {}

int CAttachableObject::AttemptUpdate(float dt) {
    return CStaticObject::AttemptUpdate(dt);
}

int CAttachableObject::CommitUpdate() {
    CStaticObject::CommitUpdate();
    for (int i = 0; i < MAX_ATTACHED; i++) {
        if (m_attached[i] != NULL) {
            MoveAttached(this, i);
        }
    }
    END_RETURN_NOTHING
}
