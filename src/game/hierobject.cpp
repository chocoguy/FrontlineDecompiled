// Hierarchical objects: trees of static objects (tank hulls, turrets and
// guns). Each part keeps its position and rotation relative to its parent's
// hard point; the root keeps the part table and a CSG volume over the parts.
#include "game/hierobject.h"

#include "game/staticmesh.h"
#include "game/staticobjfactory.h"
#include "game/objcreate.h"
#include "game/collision.h"
#include "game/SClsnContact.h"
#include "game/box.h"
#include "game/player.h"
#include "game/scene.h"
#include "game/Moh2.h"

CHierObject::CHierObject() : m_colliders(-1) {}

CHierObject::CHierObject(CStaticMesh* mesh, int numAnimFrames, int animFrameTime,
                         int numMotionFrames, int motionFrameTime, int motionRelative,
                         CStaticObject::MotionFrame* motion)
    : CAttachableObject(mesh, numAnimFrames, animFrameTime, numMotionFrames, motionFrameTime,
                        motionRelative, motion),
      m_colliders(-1) {}

CHierObject::~CHierObject() {}

// Links the part into `root`'s tree under part `parentIndex`, at hard point
// `attachPoint` of that part's mesh. A root passes itself.
int CHierObject::Init(CHierObject* root, int index, int parentIndex, int attachPoint,
                      CVector3& pos) {
    CStaticObject::Init();
    m_isRoot = root == this;
    m_pParent = root;
    m_pFirstChild = NULL;
    m_pNextSibling = NULL;
    m_collided = 0;
    m_noUpdate = 0;
    if (m_isRoot) {
        for (int i = 0; i < MAX_PARTS; i++) {
            m_parts[i] = NULL;
        }
    }
    m_parentTM.Ident();
    m_index = index;
    if (!m_isRoot) {
        // Like the original, a missing hard point leaves the matrix unset.
        CMatrix tm;
        StaticMesh* data = root->m_parts[parentIndex]->m_pMesh->m_pData;
        StaticMeshHardPoint* point = data->m_hardPoints;
        int i;
        for (i = 0; i < data->m_numHardPoints; i++, point++) {
            if (point->m_id == attachPoint) {
                tm = point->m_tm;
                break;
            }
        }
        if (i >= data->m_numHardPoints) {
            DebugMsg("The specified attachpoint %d was not found\n", attachPoint);
        }
        m_attachTM = tm;
        root->m_parts[index] = this;
        CHierObject* parent = root->m_parts[parentIndex];
        if (parent->m_pFirstChild != NULL) {
            m_pNextSibling = parent->m_pFirstChild;
        }
        parent->m_pFirstChild = this;
    } else {
        m_attachTM.Ident();
    }
    m_localPos.Copy(pos);
    if (m_isRoot) {
        m_parts[0] = this;
        m_csg.Init(MAX_PARTS, this);
    }
    m_colliders.reserve(m_colliderBuffer, 0x20);
    m_damageClasses[0].m_min = -1;
    m_damageClasses[1].m_min = -1;
    END_RETURN_NOTHING
}

void CHierObject::SetRotation(CQuaternion& rot) {
    m_rotation = rot;
}

CStaticObject* CHierObject::GetSubObject(int index) {
    return m_parts[index];
}

void CHierObject::GetAttachPointMatrix(int id, CMatrix& tm) {
    StaticMesh* data = m_pMesh->m_pData;
    StaticMeshHardPoint* point = data->m_hardPoints;
    for (int i = 0; i < data->m_numHardPoints; i++, point++) {
        if (point->m_id == id) {
            tm = point->m_tm;
            return;
        }
    }
    DebugMsg("The specified attachpoint %d was not found\n", id);
}

bool CHierObject::RemoveObjectFromTree(CHierObject* part, int index) {
    if (m_isRoot) {
        m_parts[index] = NULL;
    }
    if (part == m_pFirstChild) {
        m_pFirstChild = part->m_pNextSibling;
        return true;
    }
    CHierObject* prev = NULL;
    for (CHierObject* child = m_pFirstChild; child != NULL; child = child->m_pNextSibling) {
        if (child == part) {
            prev->m_pNextSibling = child->m_pNextSibling;
            return true;
        }
        if (child->RemoveObjectFromTree(part, index)) {
            return true;
        }
        prev = child;
    }
    return false;
}

// Frees the part and everything under it.
void CHierObject::CleanUp() {
    CHierObject* child = m_pFirstChild;
    while (child != NULL) {
        CHierObject* next = child->m_pNextSibling;
        child->CleanUp();
        child = next;
    }
    if (m_isRoot) {
        m_parts[0] = NULL;
    } else {
        m_pParent->RemoveObjectFromTree(this, m_index);
    }
    FreeBoxVolume(m_pWorldVolume);
    FreeBoxVolume(m_pLocalVolume);
    CStaticObjectFactory::DeallocateStaticObject(this);
}

int CHierObject::BeginUpdate(float dt) {
    if (!m_noUpdate) {
        DoBeginUpdate(dt, true);
    }
    END_RETURN_NOTHING
}

// Rebuilds the world matrix from the local rotation and position.
static inline void UpdateFromLocal(CHierObject* obj) {
    obj->m_rotation.GetMatrix(obj->m_tm);
    obj->m_tm.SetPos(obj->m_localPos);
    obj->m_tm.Multiply(obj->m_tm, obj->m_attachTM);
    obj->m_tm.Multiply(obj->m_tm, obj->m_parentTM);
}

// Mounted guns are left to the player when updated from their parent.
void CHierObject::DoBeginUpdate(float dt, bool fromParent) {
    if (fromParent && m_isMountedGun) {
        return;
    }
    AnimUpdate(dt);
    if (IsPathMoving()) {
        BeginPathMotionUpdate(dt);
    } else if (IsScriptMoving()) {
        BeginScriptMotionUpdate(dt);
    } else if (m_motionPlaying) {
        BeginAnimMotionUpdate(dt);
    } else {
        UpdateFromLocal(this);
        if (m_pWorldVolume != NULL && m_pLocalVolume != NULL) {
            m_pWorldVolume->TransformedCopy(*m_pLocalVolume, m_tm);
        }
    }
    UpdateAttachedObjects();
    for (CHierObject* child = m_pFirstChild; child != NULL; child = child->m_pNextSibling) {
        child->SetInheritedMatrix(m_tm);
        child->DoBeginUpdate(dt, true);
    }
}

CVector3 CHierObject::GetWorldLinearVelocity() const {
    if (IsPathMoving()) {
        CVector3 front = *(const CVector3*)m_tm.m[1];
        CVector3 v;
        v.x = m_speed * front.x;
        v.y = m_speed * front.y;
        v.z = m_speed * front.z;
        return v;
    }
    return m_velocity;
}

int CHierObject::AttemptUpdate(float dt) {
    if (IsPathMoving()) {
        AttemptPathMotionUpdate(dt);
    } else if (IsScriptMoving()) {
        AttemptScriptMotionUpdate(dt);
    } else if (m_motionPlaying) {
        AttemptAnimMotionUpdate(dt);
    } else {
        UpdateFromLocal(this);
    }
    for (CHierObject* child = m_pFirstChild; child != NULL; child = child->m_pNextSibling) {
        child->m_parentTM = m_tm;
        child->AttemptUpdate(dt);
    }
    if (m_isMountedGun) {
        g_scene.GetPlayer(0)->SetPlayersMountedMGPositions(&m_tm);
    }
    if (m_isRoot) {
        CVector3 min;
        CVector3 max;
        m_csg.GetExtents(min, max);
        SetExtents(min, max);
    }
    END_RETURN_NOTHING
}

int CHierObject::CommitUpdate() {
    ConstrainVelocity();
    if (m_pWorldBound != m_pWorldVolume) {
        m_pWorldBound->TransformedCopy(*m_pLocalVolume, m_tm);
    }
    if (IsPathMoving()) {
        CommitPathMotionUpdate();
        m_localPos = *(const CVector3*)m_tm.m[3];
        m_rotation.SetFromMatrix(m_tm);
    } else if (IsScriptMoving()) {
        CommitScriptMotionUpdate();
    } else if (m_motionPlaying) {
        CommitAnimMotionUpdate();
    }
    for (CHierObject* child = m_pFirstChild; child != NULL; child = child->m_pNextSibling) {
        child->CommitUpdate();
    }
    m_collided = 0;
    END_RETURN_NOTHING
}

// Parts only translate and rotate; the events carry the part index.
void CHierObject::BeginScriptMotionUpdate(float) {
    if (m_translating && m_translateTime <= 0.0f) {
        TriggerScriptEvent(m_translateEvent, ADDR_TO_PTR(void, m_index), false);
        m_translateTime = 0.0f;
        m_translating = 0;
        m_velocity.z = 0.0f;
        m_velocity.y = 0.0f;
        m_velocity.x = 0.0f;
    }
    if (m_rotating) {
        m_rotation.Normalize();
        if (m_rotateTime <= 0.0f) {
            TriggerScriptEvent(m_rotateEvent, ADDR_TO_PTR(void, m_index), false);
            m_rotateTime = 0.0f;
            m_rotating = 0;
            m_angularVelocity.Set(0.0f, 0.0f, 0.0f, 1.0f);
        }
    }
}

void CHierObject::AttemptScriptMotionUpdate(float dt) {
    float translateStep = dt;
    float rotateStep = dt;
    if (m_rotating) {
        float step = m_rotateTime;
        if (!(step < dt)) {
            step = dt;
        }
        rotateStep = step;
        if (step < 1.0f) {
            CQuaternion identity;
            identity.w = 1.0f;
            identity.x = 0.0f;
            identity.y = 0.0f;
            identity.z = 0.0f;
            m_newRotation.Slerp(m_angularVelocity, identity, step);
        } else {
            m_newRotation = m_angularVelocity;
        }
        m_newRotation.Multiply(m_rotation);
    } else {
        m_newRotation = m_rotation;
    }
    m_newRotation.GetMatrix(m_tm);
    if (m_translating) {
        float step = m_translateTime;
        if (!(step < dt)) {
            step = dt;
        }
        translateStep = step;
        m_move.x = step * m_velocity.x;
        m_move.y = step * m_velocity.y;
        m_move.z = step * m_velocity.z;
        m_tm.TransformPoint(m_move, m_move);
    } else {
        m_move.z = 0.0f;
        m_move.y = 0.0f;
        m_move.x = 0.0f;
    }
    m_tm.SetPos(m_move);
    m_tm.Translate(m_localPos);
    m_tm.Multiply(m_tm, m_attachTM);
    m_tm.Multiply(m_tm, m_parentTM);
    m_translateStep = translateStep;
    m_rotateStep = rotateStep;
    if (m_pWorldVolume != NULL && m_pLocalVolume != NULL) {
        m_pWorldVolume->TransformedCopy(*m_pLocalVolume, m_tm);
    }
}

void CHierObject::CommitScriptMotionUpdate() {
    if (m_translating) {
        float time = 0.0f;
        m_translateTime -= m_translateStep;
        if (!(m_translateTime < 0.0f)) {
            time = m_translateTime;
        }
        m_translateTime = time;
        m_localPos.x = m_localPos.x + m_move.x;
        m_localPos.y = m_localPos.y + m_move.y;
        m_localPos.z = m_localPos.z + m_move.z;
    }
    if (m_rotating) {
        float time = 0.0f;
        m_rotation = m_newRotation;
        m_rotateTime -= m_rotateStep;
        if (!(m_rotateTime < 0.0f)) {
            time = m_rotateTime;
        }
        m_rotateTime = time;
    }
}

// As CStaticObject's, from the local position and rotation.
void CHierObject::BeginAnimMotionUpdate(float dt) {
    if (m_numMotionFrames == 0 || m_motionFrameTime == 0) {
        return;
    }
    m_motionPos = m_localPos;
    m_motionRot = m_rotation;
    m_motionTime += dt;
    while (m_motionTime >= m_motionFrameTime) {
        m_motionTime -= m_motionFrameTime;
        m_motionFrame++;
    }

    int frame = m_motionFrame;
    int next = frame + 1;
    if (m_motionEnd != 0) {
        if (next >= m_motionEnd) {
            next = m_motionEnd - 1;
        }
        if (frame >= m_motionEnd) {
            if (m_motionEvent != -1) {
                TriggerScriptEvent(m_motionEvent, NULL, false);
                m_motionEvent = (EBSEventEnum)-1;
            }
            m_motionFrame = m_motionEnd - 1;
            m_motionPlaying = 0;
        }
    } else {
        m_motionFrame %= m_numMotionFrames;
        next = (frame + 1) % m_numMotionFrames;
    }

    float t = m_motionTime / m_motionFrameTime;
    CVector3 from = m_pMotion[m_motionFrame].m_pos;
    CVector3 to = m_pMotion[next].m_pos;
    if (t < 0.0f) {
        t = 0.0f;
    }
    if (t > 1.0f) {
        t = 1.0f;
    }
    CVector3 pos;
    pos.x = t * (to.x - from.x) + from.x;
    pos.y = t * (to.y - from.y) + from.y;
    pos.z = t * (to.z - from.z) + from.z;
    CQuaternion fromRot = m_pMotion[m_motionFrame].m_rot;
    CQuaternion toRot = m_pMotion[next].m_rot;
    CQuaternion rot;
    rot.Slerp(toRot, fromRot, t);
    if (m_motionRelative) {
        m_motionBaseRot.Rotate(pos, pos);
        pos.x += m_motionBasePos.x;
        pos.y += m_motionBasePos.y;
        pos.z += m_motionBasePos.z;
        rot.Multiply(m_motionBaseRot);
    }

    float inv = 1.0f / dt;
    m_velocity.x = pos.x - m_motionPos.x;
    m_velocity.y = pos.y - m_motionPos.y;
    m_velocity.z = pos.z - m_motionPos.z;
    m_motionTargetRot = rot;
    m_velocity.x = m_velocity.x * inv;
    m_velocity.y = m_velocity.y * inv;
    m_velocity.z = m_velocity.z * inv;
    m_invStep = inv;
}

void CHierObject::AttemptAnimMotionUpdate(float dt) {
    if (m_numMotionFrames != 0 && m_motionFrameTime != 0) {
        m_move.x = dt * m_velocity.x;
        m_move.y = dt * m_velocity.y;
        m_move.z = dt * m_velocity.z;
        CQuaternion rot;
        rot.Slerp(m_motionTargetRot, m_motionRot, dt * m_invStep);
        m_newRotation = rot;
        rot.GetMatrix(m_tm);
        if (m_motionRelative) {
            m_tm.TransformPoint(m_move, m_move);
        }
        m_tm.SetPos(m_motionPos);
        m_tm.Translate(m_move);
        m_tm.Multiply(m_tm, m_attachTM);
        m_tm.Multiply(m_tm, m_parentTM);
    }
    if (m_pWorldVolume != NULL && m_pLocalVolume != NULL) {
        m_pWorldVolume->TransformedCopy(*m_pLocalVolume, m_tm);
    }
}

void CHierObject::CommitAnimMotionUpdate() {
    if (m_numMotionFrames != 0 && m_motionFrameTime != 0) {
        m_rotation = m_newRotation;
        m_localPos.x = m_localPos.x + m_move.x;
        m_localPos.y = m_localPos.y + m_move.y;
        m_localPos.z = m_localPos.z + m_move.z;
        m_motionPos.Copy(m_localPos);
        m_motionRot = m_rotation;
    }
}

// Flags part `index` and the parts above it as hit.
bool CHierObject::SetHierCollisionState(int index) {
    if (index == m_index) {
        m_collided = 1;
        return true;
    }
    for (CHierObject* child = m_pFirstChild; child != NULL; child = child->m_pNextSibling) {
        if (child->SetHierCollisionState(index)) {
            m_collided = 1;
            return true;
        }
    }
    return false;
}

// Puts the part back where it was at the start of the frame.
static inline void UndoMotion(CHierObject* obj) {
    obj->m_translateStep = 0.0f;
    obj->m_rotateStep = 0.0f;
    obj->m_newRotation = obj->m_rotation;
    obj->m_move.z = 0.0f;
    obj->m_move.y = 0.0f;
    obj->m_move.x = 0.0f;
    if (obj->IsPathMoving()) {
        obj->m_tm.SetPos(obj->m_lastPos);
        obj->m_lastSpeed = 0.0f;
        obj->m_speed = 0.0f;
    } else if (obj->m_translating || obj->m_rotating) {
        obj->m_rotation.GetMatrix(obj->m_tm);
        obj->m_tm.Translate(obj->m_localPos);
        obj->m_tm.Multiply(obj->m_tm, obj->m_attachTM);
        obj->m_tm.Multiply(obj->m_tm, obj->m_parentTM);
    } else if (obj->m_motionPlaying) {
        obj->m_velocity.z = 0.0f;
        obj->m_velocity.y = 0.0f;
        obj->m_velocity.x = 0.0f;
        obj->m_motionTargetRot = obj->m_motionRot;
    }
}

// The root flags the parts that were hit. A hit part pushed by the player
// stops if it was moving into them; characters it hits are collected for
// ConstrainVelocity.
void CHierObject::OnCollision(const CCollision& clsn) {
    if (m_noUpdate) {
        return;
    }
    ISceneNode* other = clsn.m_pNode0;
    if (other == this) {
        other = clsn.m_pNode1;
    }
    if (other->AsBullet() != NULL) {
        return;
    }
    if (m_isRoot) {
        for (SClsnContact* c = clsn.m_pContacts; c != NULL; c = c->m_pNext) {
            SetHierCollisionState((int)PTR_TO_ADDR(c->m_pPoly));
        }
        AttemptCollisionDamage(clsn);
    }
    if (m_collided) {
        if (m_isRoot && other->AsPlayerObject() != NULL) {
            CVector3 point = clsn.m_pContacts->m_point;
            CVector3 playerPos;
            other->GetPosition(playerPos);
            CVector3 toPoint;
            toPoint.x = point.x - playerPos.x;
            toPoint.y = point.y - playerPos.y;
            toPoint.z = point.z - playerPos.z;
            toPoint.Normalize();
            // The original leaves this unset (stale registers) when the
            // object is neither on a path nor script moving.
            CVector3 motion;
            motion.x = 0.0f;
            motion.y = 0.0f;
            motion.z = 0.0f;
            if (IsPathMoving()) {
                CVector3 pos = *(const CVector3*)m_tm.m[3];
                motion.x = pos.x - m_lastPos.x;
                motion.y = pos.y - m_lastPos.y;
                motion.z = pos.z - m_lastPos.z;
                motion.Normalize();
            } else if (IsScriptMoving()) {
                CMatrix m;
                CVector3 oldPoint;
                m.FastInverse(m_tm);
                m.TransformPoint(oldPoint, point);
                m_rotation.GetMatrix(m);
                m.SetPos(m_lastPos);
                m.TransformPoint(oldPoint, oldPoint);
                motion.x = point.x - oldPoint.x;
                motion.y = point.y - oldPoint.y;
                motion.z = point.z - oldPoint.z;
                motion.Normalize();
            }
            if (toPoint.z * motion.z + (toPoint.x * motion.x + toPoint.y * motion.y) < 0.0f) {
                UndoMotion(this);
                for (CHierObject* child = m_pFirstChild; child != NULL;
                     child = child->m_pNextSibling) {
                    child->m_parentTM = m_tm;
                    child->OnCollision(clsn);
                }
            }
            m_collided = 0;
            return;
        }
        for (SClsnContact* c = clsn.m_pContacts; c != NULL; c = c->m_pNext) {
            CAnimObject* anim = other->AsAnimObject();
            if (m_colliders.m_size < m_colliders.m_capacity) {
                m_colliders.push_back(anim);
            }
        }
    }
    for (CHierObject* child = m_pFirstChild; child != NULL; child = child->m_pNextSibling) {
        child->OnCollision(clsn);
    }
}

// A hit part stops the characters it ran into and its own motion.
void CHierObject::ConstrainVelocity() {
    if (m_collided) {
        for (CAnimObject** it = m_colliders.begin(); it != m_colliders.end(); it++) {
            if (*it != NULL) {
                ((ISceneNode*)*it)->ConstrainVelocity();
            }
        }
        UndoMotion(this);
    }
    for (CHierObject* child = m_pFirstChild; child != NULL; child = child->m_pNextSibling) {
        child->m_parentTM = m_tm;
        child->ConstrainVelocity();
    }
}

const IVolume* CHierObject::GetSubVolume(int index) const {
    if (m_isRoot && index > 0) {
        CHierObject* part = m_parts[index];
        if (part != NULL) {
            return part->GetSubVolume(index);
        }
        return NULL;
    }
    return m_pWorldVolume;
}

const IVolume* CHierObject::GetWorldBoundingVolume(ISceneNode::EVolumeType type) const {
    switch ((int)type) {
    case 3:
    case 0:
    case 1:
    case 2:
        if (m_isRoot) {
            return &m_csg;
        }
        return m_pWorldVolume;
    default:
        return NULL;
    }
}

void CHierObject::SetInheritedMatrix(CMatrix& tm) {
    m_parentTM = tm;
}

void CHierObject::Draw(CDrawContext& ctx) {
    CStaticObject::Draw(ctx);
    for (CHierObject* child = m_pFirstChild; child != NULL; child = child->m_pNextSibling) {
        child->Draw(ctx);
    }
}

// Replaces the part's mesh and motion (damage states) and refits its box.
void CHierObject::SwapOut(CStaticMesh* mesh, MotionData* motion) {
    m_pMesh = mesh;
    if (motion != NULL) {
        m_numAnimFrames = ((CStaticAnimMesh*)mesh)->numFrames();
        m_animFrameTime = ((CStaticAnimMesh*)mesh)->ticksPerFrame();
        m_numMotionFrames = motion->m_numFrames;
        m_motionFrameTime = motion->m_frameTime;
        m_motionRelative = motion->m_relative != 0;
        m_pMotion = motion->m_frames;
    } else {
        m_numAnimFrames = 0;
        m_animFrameTime = 0;
        m_numMotionFrames = 0;
        m_motionFrameTime = 0;
        m_motionRelative = 0;
        m_pMotion = NULL;
    }

    MSHNode* bounds = m_pMesh->m_pData->m_nodes;
    CVector3 axisX = bounds->m_axisX;
    CVolBox* box = (CVolBox*)m_pLocalVolume;
    CVector3 axisY = bounds->m_axisY;
    CVector3 axisZ = bounds->m_axisZ;
    CVector3 center = bounds->m_center;
    if (m_pParent->AsHierTankObject() != NULL) {
        // Tank parts get a tighter box, shifted forward and down.
        float lenX = axisX.Length();
        float lenZ = axisZ.Length();
        center.x = center.x + 0.0f;
        float lenY = axisY.Length();
        center.y += 0.1f * lenZ;
        center.z = -0.1f * lenY + center.z;
        box->SetWidth(1.6f * lenX);
        box->SetHeight(1.7f * lenZ);
        box->SetDepth(1.4f * lenY);
    } else {
        box->SetWidth(2.0f * axisX.Length());
        box->SetHeight(2.0f * axisZ.Length());
        box->SetDepth(2.0f * axisY.Length());
    }
    axisX.Normalize();
    axisY.Normalize();
    axisZ.Normalize();
    box->SetBasis(axisX, axisY, axisZ);
    box->SetCenter(center);
}

BPDLightVolume* CHierObject::GetLightVolume() {
    if (m_isRoot) {
        return m_lightVolumes.GetVolume();
    }
    return m_pParent->GetLightVolume();
}

// Motion clips on parts are always relative to the part's local frame.
void CHierObject::StartMotionPlayback(int start, int end, EBSEventEnum event) {
    if (m_pMotion == NULL) {
        return;
    }
    if (start < 0) {
        start = 0;
    }
    if (start >= m_numMotionFrames) {
        start = m_numMotionFrames - 1;
    }
    if (end < 0) {
        end = 0;
    }
    if (end >= m_numMotionFrames) {
        end = m_numMotionFrames;
    }
    m_motionStart = start;
    m_motionFrame = start;
    m_motionEnd = end;
    m_motionEvent = event;
    m_motionBaseRot = m_rotation;
    m_motionBasePos.Copy(m_localPos);
    m_motionPlaying = 1;
}

// Sets the multiplier for damage classes min..max on part `index` (two
// ranges per part).
void CHierObject::RegisterBulletDamage(int index, int min, int max, float multiplier) {
    CHierObject* part = index > 0 ? m_parts[index] : this;
    for (int i = 0; i < 2; i++) {
        if (part->m_damageClasses[i].m_min == -1) {
            part->m_damageClasses[i].m_min = min;
            part->m_damageClasses[i].m_max = max;
            part->m_damageClasses[i].m_multiplier = multiplier;
            return;
        }
    }
}

float CHierObject::GetBulletDamageMultiplierByClass(int index, int damageClass) {
    CHierObject* part = index > 0 ? m_parts[index] : this;
    if (damageClass >= part->m_damageClasses[0].m_min &&
        damageClass <= part->m_damageClasses[0].m_max) {
        return part->m_damageClasses[0].m_multiplier;
    }
    if (damageClass >= part->m_damageClasses[1].m_min &&
        damageClass <= part->m_damageClasses[1].m_max) {
        return part->m_damageClasses[1].m_multiplier;
    }
    return 0.0f;
}

CHierObject* CHierObject::AsHierObject() {
    return this;
}

const CHierObject* CHierObject::AsHierObject() const {
    return this;
}

int CHierObject::GetObjectType() const {
    return 8;
}

int CHierObject::Init() {
    CStaticObject::Init();
    END_RETURN_NOTHING
}
