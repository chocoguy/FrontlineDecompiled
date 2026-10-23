// The level collision database as a volume: walks the BSP with an
// explicit stack, rejects leaves by their oriented boxes and tests the
// triangle groups found there. Groups are marked visited by negating their
// counts and restored when a query finishes.
#include "game/cdbobject.h"
#include "game/cdbgeom.h"
#include "game/capsule.h"
#include "game/sphere.h"
#include "game/animated_volume.h"
#include "game/fiber.h"
#include "game/plane.h"
#include "game/line.h"
#include "game/csg_volume.h"
#include "game/collision.h"
#include "game/SClsnContact.h"
#include "game/debug_prof.h"
#include "game/offsetptr.h"
#include "dolphin/os.h"

static u8 g_bDisablePassThru = 1;

// A line query's pending branch with the part of the line on its side.
struct SLocalFrame {
    SLocalFrame() {}
    ~SLocalFrame() {}
    /* 0x00 */ const CDBBranchNode* m_node;
    /* 0x04 */ u8 unk_0x4[0xC];
    /* 0x10 */ CLine3 m_line;
    /* 0x4C */ u8 unk_0x4C[0x4];
};

// A branch's split plane in CPlane's layout (the original copied it
// without running CPlane's constructor).
struct SPlaneData {
    /* 0x00 */ f32 m_normal[3];
    /* 0x0C */ f32 unk_0xC;
    /* 0x10 */ f32 m_d;
};

static inline const CPlane& BranchPlane(const CDBBranchNode& node, SPlaneData& data) {
    data.m_normal[0] = node.m_normal[0];
    data.m_normal[1] = node.m_normal[1];
    data.m_normal[2] = node.m_normal[2];
    data.m_d = node.m_d;
    return *(const CPlane*)&data;
}


// Signed distance from the plane to the volume: positive when wholly in
// front, negative when wholly behind, zero when straddling.
static inline float GetDistanceFromPlane(const CPlane& plane, const CVolSphere& sphere) {
    CVector3 center = sphere.GetCenter();
    float dist = (plane.m_normal[0] * center.x + plane.m_normal[1] * center.y + plane.m_normal[2] * center.z) -
                 plane.m_d;
    float radius = sphere.GetRadius();
    float result = dist - radius;
    if (result > 0.0f) {
        return result;
    }
    result = dist + radius;
    if (result < 0.0f) {
        return result;
    }
    return 0.0f;
}

static inline float GetDistanceFromPlane(const CPlane& plane, const CVolBox& box) {
    float nx = plane.m_normal[0];
    float ny = plane.m_normal[1];
    float nz = plane.m_normal[2];
    float radius = (float)fabs(box.m_axisX.x * nx + box.m_axisX.y * ny + box.m_axisX.z * nz) * box.m_halfSize[0] +
                   (float)fabs(box.m_axisY.x * nx + box.m_axisY.y * ny + box.m_axisY.z * nz) * box.m_halfSize[1] +
                   (float)fabs(box.m_axisZ.x * nx + box.m_axisZ.y * ny + box.m_axisZ.z * nz) * box.m_halfSize[2];
    CVector3 center = box.m_center;
    float dist = (nx * center.x + ny * center.y + nz * center.z) - plane.m_d;
    float result = dist - radius;
    if (result > 0.0f) {
        return result;
    }
    result = dist + radius;
    if (result < 0.0f) {
        return result;
    }
    return 0.0f;
}

// A capsule wholly on one side uses the sphere at its nearer end.
static float GetDistanceFromPlane(const CPlane& plane, const CVolCapsule& capsule) {
    CVector3 start = capsule.m_line.m_start;
    CVector3 end = capsule.m_line.m_end;
    float radius = capsule.m_radius;
    float d0 = (plane.m_normal[0] * start.x + plane.m_normal[1] * start.y + plane.m_normal[2] * start.z) - plane.m_d;
    float d1 = (plane.m_normal[0] * end.x + plane.m_normal[1] * end.y + plane.m_normal[2] * end.z) - plane.m_d;
    if (d0 * d1 > radius * radius) {
        if ((float)fabs(d0) <= (float)fabs(d1)) {
            CVolSphere sphere(start, radius);
            return GetDistanceFromPlane(plane, sphere);
        } else {
            CVolSphere sphere(end, radius);
            return GetDistanceFromPlane(plane, sphere);
        }
    }
    return 0.0f;
}

template <class T>
int CCDBObject::TestCDBLeafNodeCollision(const CDBLeafNode& leaf, const T& volume, CCollision& collision, bool,
                                         dwi::fast_vec<CDBTriGroup*>& groups) const {
    OSGetTick();
    CCDBVolBox box(leaf.m_box);
    int result = box.TestCollision(volume, collision, false);
    if (result != CLSN_NONE) {
        for (int i = 0; i < leaf.m_count; i++) {
            CDBTriGroup* group = leaf.m_groups[i];
            if (group->m_count > 0) {
                group->m_count = -group->m_count;
                groups.push_back(group);
            }
        }
    }
    OSGetTick();
    return result;
}

template <class T>
int CCDBObject::TestCDBBranchNodeCollision(const CDBBranchNode& root, const T& volume, CCollision& collision,
                                           bool contacts, dwi::fast_vec<CDBTriGroup*>& groups) const {
    OSGetTick();
    int result = CLSN_NONE;
    const CDBBranchNode* stackBuffer[20];
    dwi::fast_vec<const CDBBranchNode*> stack(0xB);
    stack.reserve(stackBuffer, 20);
    stack.push_back(&root);
    while (stack.size() != 0) {
        const CDBBranchNode* node = stack.back();
        stack.pop_back();
        SPlaneData plane;
        float dist = GetDistanceFromPlane(BranchPlane(*node, plane), volume);
        const CDBNode* child = node->m_front;
        if (child != NULL && dist >= 0.0f) {
            if (child->m_type == CDBNODE_BRANCH) {
                stack.push_back((const CDBBranchNode*)child);
            } else {
                int leafResult =
                    TestCDBLeafNodeCollision(*(const CDBLeafNode*)child, volume, collision, contacts, groups);
                if (leafResult > result) {
                    result = leafResult;
                }
            }
        }
        child = node->m_back;
        if (child != NULL && dist <= 0.0f) {
            if (child->m_type == CDBNODE_BRANCH) {
                stack.push_back((const CDBBranchNode*)child);
            } else {
                int leafResult =
                    TestCDBLeafNodeCollision(*(const CDBLeafNode*)child, volume, collision, contacts, groups);
                if (leafResult > result) {
                    result = leafResult;
                }
            }
        }
    }
    OSGetTick();
    return result;
}

// Tests the volume against every collected group whose plane it reaches.
template <class T>
int TestCollisionWithTriGroups(const dwi::fast_vec<CDBTriGroup*>& groups, const T& volume, CCollision& collision,
                               bool contacts) {
    int result = CLSN_NONE;
    for (CDBTriGroup* const* it = groups.m_data; it != groups.m_data + groups.m_size; it++) {
        const CDBTriGroup* group = *it;
        CPlane plane(CVector3(group->m_normal[0], group->m_normal[1], group->m_normal[2]), group->m_d);
        if (volume.TestCollision(plane, collision, false) != CLSN_NONE) {
            for (int i = 0; i < -group->m_count; i++) {
                const CDBTri* t = group->m_tris[i];
                if (g_bDisablePassThru & t->m_passThru) {
                    break;
                }
                CCDBTriangle tri(*t);
                int triResult = tri.TestCollision(volume, collision, t, contacts);
                if (triResult > result) {
                    result = triResult;
                    if (result == CLSN_PENETRATING) {
                        return result;
                    }
                }
            }
        }
    }
    return result;
}

// Collects the triangles of a leaf the sphere touches (no duplicates);
// stops when `tris` is full.
static inline void AddIntersectingTris(const CDBLeafNode& leaf, const CVolSphere& sphere,
                                       dwi::fast_vec<const CDBTri*>& tris) {
    CCDBVolBox box(leaf.m_box);
    CCollision collision;
    if (box.TestCollision(sphere, collision, false) != CLSN_NONE) {
        for (int i = 0; i < leaf.m_count; i++) {
            const CDBTriGroup* group = leaf.m_groups[i];
            for (int j = 0; j < group->m_count; j++) {
                if (tris.full()) {
                    return;
                }
                const CDBTri* t = group->m_tris[j];
                CCDBTriangle tri(*t);
                if (tri.TestCollision(sphere, collision, t, false) != CLSN_NONE) {
                    const CDBTri** it = tris.begin();
                    const CDBTri** end = tris.end();
                    while (it != end && *it != t) {
                        it++;
                    }
                    if (it == end) {
                        tris.push_back(t);
                    }
                }
            }
        }
    }
}

void CCDBObject::GetIntersectingTris(dwi::fast_vec<const CDBTri*>& tris, const CVolSphere& sphere) const {
    OSGetTick();
    const CDBBranchNode* stackBuffer[20];
    dwi::fast_vec<const CDBBranchNode*> stack(0xB);
    stack.reserve(stackBuffer, 20);
    stack.push_back(m_cdb->m_branches);
    while (stack.size() != 0) {
        const CDBBranchNode* node = stack.back();
        stack.pop_back();
        SPlaneData plane;
        float dist = GetDistanceFromPlane(BranchPlane(*node, plane), sphere);
        const CDBNode* child = node->m_front;
        if (child != NULL && dist >= 0.0f) {
            if (child->m_type == CDBNODE_BRANCH) {
                stack.push_back((const CDBBranchNode*)child);
            } else {
                AddIntersectingTris(*(const CDBLeafNode*)child, sphere, tris);
            }
        }
        child = node->m_back;
        if (child != NULL && dist <= 0.0f) {
            if (child->m_type == CDBNODE_BRANCH) {
                stack.push_back((const CDBBranchNode*)child);
            } else {
                AddIntersectingTris(*(const CDBLeafNode*)child, sphere, tris);
            }
        }
    }
    OSGetTick();
}

// The groups of a query get their counts back.
static inline void RestoreTriGroups(dwi::fast_vec<CDBTriGroup*>& groups) {
    for (CDBTriGroup** it = groups.begin(); it != groups.end(); it++) {
        (*it)->m_count = -(*it)->m_count;
    }
}

#ifdef FRONTLINE_NATIVE
// Standard C++ wants the line specialization (defined below) declared before
// its first use; CodeWarrior didn't.
template <>
int CCDBObject::TestCDBBranchNodeCollision<CLine3>(const CDBBranchNode& root, const CLine3& line,
                                                   CCollision& collision, bool contacts,
                                                   dwi::fast_vec<CDBTriGroup*>& groups) const;
#endif

int CCDBObject::TestCollision(const CLine3& line, CCollision& collision, bool contacts) const {
    PROFILE_BEGIN(PROFBIN_CDB_LINE);
    OSGetTick();
    CDBTriGroup* buffer[0x800];
    dwi::fast_vec<CDBTriGroup*> groups(0xC);
    groups.reserve(buffer, 0x800);
    int result = TestCDBBranchNodeCollision(*m_cdb->m_branches, line, collision, contacts, groups);
    RestoreTriGroups(groups);
    OSGetTick();
    PROFILE_END(PROFBIN_CDB_LINE);
    return result;
}

// Fibers hit every polygon; the first contact records this object.
int CCDBObject::TestCollision(const CVolFiber& fiber, CCollision& collision, bool contacts) const {
    PROFILE_BEGIN(PROFBIN_CDB_FIBER);
    collision.m_flags |= 0x80;
    int result = TestCollision(fiber.GetLine(), collision, contacts);
    if (result == CLSN_TOUCHING) {
        SClsnContact* contact = collision.m_pContacts;
        if (contact != NULL) {
            contact->m_pVolume = this;
        }
    }
    PROFILE_END(PROFBIN_CDB_FIBER);
    return result;
}

void CCDBObject::GetExtents(CVector3& min, CVector3& max) const {
    min = m_min;
    max = m_max;
}

// The line walk splits the line at each branch plane to choose the
// children, but tests leaves (boxes and groups) against the whole line and
// stops at the first penetration.
template <>
int CCDBObject::TestCDBBranchNodeCollision<CLine3>(const CDBBranchNode& root, const CLine3& line,
                                                   CCollision& collision, bool contacts,
                                                   dwi::fast_vec<CDBTriGroup*>& groups) const {
    int result = CLSN_NONE;
    OSGetTick();
    SLocalFrame frameBuffer[20];
    dwi::fast_vec<SLocalFrame> stack(0xB);
    stack.reserve(frameBuffer, 20);
    SLocalFrame frame;
    frame.m_node = &root;
    frame.m_line = line;
    stack.push_back(frame);
    while (stack.size() != 0) {
        const CDBNode* farNode = NULL;
        SLocalFrame current = stack.back();
        stack.pop_back();
        const CDBBranchNode* node = current.m_node;
        CVector3 start = current.m_line.m_start;
        CVector3 end = current.m_line.m_end;
        CVector3 dir = current.m_line.m_dir;
        float d0 = (node->m_normal[0] * start.x + node->m_normal[1] * start.y + node->m_normal[2] * start.z) - node->m_d;
        float d1 = (node->m_normal[0] * end.x + node->m_normal[1] * end.y + node->m_normal[2] * end.z) - node->m_d;
        CLine3 frontSeg;
        CLine3 backSeg;
        const CDBNode* nearNode;
        if (d0 >= 0.0f) {
            nearNode = node->m_front;
            if (d1 < 0.0f) {
                farNode = node->m_back;
                float t = d0 / (d0 - d1);
                CVector3 split;
                split.x = t * dir.x + start.x;
                split.y = t * dir.y + start.y;
                split.z = t * dir.z + start.z;
                frontSeg = CLine3(start, split);
                backSeg = CLine3(split, end);
            } else {
                frontSeg = CLine3(start, end);
            }
        } else {
            nearNode = node->m_back;
            if (d1 >= 0.0f) {
                farNode = node->m_front;
                float t = -d0 / (d1 - d0);
                CVector3 split;
                split.x = t * dir.x + start.x;
                split.y = t * dir.y + start.y;
                split.z = t * dir.z + start.z;
                backSeg = CLine3(start, split);
                frontSeg = CLine3(split, end);
            } else {
                backSeg = current.m_line;
            }
        }

        const CDBNode* children[2];
        children[0] = nearNode;
        children[1] = farNode;
        for (int c = 0; c < 2; c++) {
            const CDBNode* child = children[c];
            if (child == NULL) {
                continue;
            }
            if (child->m_type == CDBNODE_BRANCH) {
                SLocalFrame next;
                next.m_node = (const CDBBranchNode*)child;
                next.m_line = child == node->m_front ? frontSeg : backSeg;
                stack.push_back(next);
                continue;
            }
            const CDBLeafNode& leaf = *(const CDBLeafNode*)child;
            OSGetTick();
            PROFILE_BEGIN(PROFBIN_CDB_LEAF);
            int leafResult;
            {
                CCDBVolBox box(leaf.m_box);
                leafResult = CLSN_NONE;
                if (box.TestCollision(line, collision, false) != CLSN_NONE) {
                    for (int i = 0; i < leaf.m_count; i++) {
                        CDBTriGroup* group = leaf.m_groups[i];
                        if (group->m_count > 0) {
                            group->m_count = -group->m_count;
                            groups.push_back(group);
                            int groupResult = TestCollisionWithTriGroup(*group, line, collision, contacts);
                            if (groupResult > leafResult) {
                                leafResult = groupResult;
                                if (leafResult == CLSN_PENETRATING) {
                                    break;
                                }
                            }
                        }
                    }
                }
                PROFILE_END(PROFBIN_CDB_LEAF);
                OSGetTick();
            }
            if (leafResult > result) {
                result = leafResult;
                if (result == CLSN_PENETRATING) {
                    goto done;
                }
            }
        }
    }
done:
    OSGetTick();
    return result;
}

int CCDBObject::TestCollision(const CCSGVolume& csg, CCollision& collision, bool contacts) const {
    collision.SwapOrder();
    return csg.TestCollision(*this, collision, contacts);
}

int CCDBObject::TestCollision(const CAnimatedVolume& volume, CCollision& collision, bool contacts) const {
    int result = CLSN_NONE;
    CDBTriGroup* buffer[0x800];
    dwi::fast_vec<CDBTriGroup*> groups(0xC);
    groups.reserve(buffer, 0x800);
    if (TestCDBBranchNodeCollision(*m_cdb->m_branches, volume, collision, contacts, groups) != CLSN_NONE) {
        result = TestCollisionWithTriGroups(groups, volume, collision, contacts);
    }
    RestoreTriGroups(groups);
    return result;
}

int CCDBObject::TestCollision(const CVolCapsule& capsule, CCollision& collision, bool contacts) const {
    int result = CLSN_NONE;
    PROFILE_BEGIN(PROFBIN_CDB_CAPSULE);
    OSGetTick();
    CDBTriGroup* buffer[0x800];
    dwi::fast_vec<CDBTriGroup*> groups(0xC);
    groups.reserve(buffer, 0x800);
    if (TestCDBBranchNodeCollision(*m_cdb->m_branches, capsule, collision, contacts, groups) != CLSN_NONE) {
        result = TestCollisionWithTriGroups(groups, capsule, collision, contacts);
    }
    RestoreTriGroups(groups);
    OSGetTick();
    PROFILE_END(PROFBIN_CDB_CAPSULE);
    return result;
}

int CCDBObject::TestCollision(const CVolSphere& sphere, CCollision& collision, bool contacts) const {
    int result = CLSN_NONE;
    PROFILE_BEGIN(PROFBIN_CDB_SPHERE);
    OSGetTick();
    CDBTriGroup* buffer[0x800];
    dwi::fast_vec<CDBTriGroup*> groups(0xC);
    groups.reserve(buffer, 0x800);
    if (TestCDBBranchNodeCollision(*m_cdb->m_branches, sphere, collision, contacts, groups) != CLSN_NONE) {
        result = TestCollisionWithTriGroups(groups, sphere, collision, contacts);
    }
    RestoreTriGroups(groups);
    OSGetTick();
    PROFILE_END(PROFBIN_CDB_SPHERE);
    return result;
}

int CCDBObject::TestCollision(const CVolBox& box, CCollision& collision, bool contacts) const {
    int result = CLSN_NONE;
    CDBTriGroup* buffer[0x800];
    dwi::fast_vec<CDBTriGroup*> groups(0xC);
    groups.reserve(buffer, 0x800);
    if (TestCDBBranchNodeCollision(*m_cdb->m_branches, box, collision, contacts, groups) != CLSN_NONE) {
        result = TestCollisionWithTriGroups(groups, box, collision, contacts);
    }
    RestoreTriGroups(groups);
    return result;
}

int CCDBObject::TestCollision(const IVolume& volume, CCollision& collision, bool contacts) const {
    collision.SwapOrder();
    return volume.TestCollision(*this, collision, contacts);
}

CCDBObject::~CCDBObject() {}

void CCDBObject::Init(CDB* cdb) {
    m_cdb = cdb;
#ifdef FRONTLINE_NATIVE
    CDBFromBigEndian(cdb);
#endif
    CDB* base = m_cdb;
    offsetPtr(base->m_tris, (int)PTR_TO_ADDR(base));
    offsetPtr(m_cdb->m_triGroups, (int)PTR_TO_ADDR(base));
    offsetPtr(m_cdb->m_leaves, (int)PTR_TO_ADDR(base));
    offsetPtr(m_cdb->m_branches, (int)PTR_TO_ADDR(base));
    offsetPtr(m_cdb->surfaceTypes, (int)PTR_TO_ADDR(base));

    CDBTriGroup* group = m_cdb->m_triGroups;
    for (int i = 0; i < m_cdb->m_numTriGroups; i++) {
        for (int j = 0; j < group->m_count; j++) {
            offsetPtr(group->m_tris[j], (int)PTR_TO_ADDR(base));
            CDBTri* tri = group->m_tris[j];
            offsetPtr(tri->m_verts[0], (int)PTR_TO_ADDR(base));
            offsetPtr(tri->m_verts[1], (int)PTR_TO_ADDR(base));
            offsetPtr(tri->m_verts[2], (int)PTR_TO_ADDR(base));
        }
        group = (CDBTriGroup*)((u8*)group + group->m_count * 4 + 0x14);
    }

    CDBLeafNode* leaf = m_cdb->m_leaves;
    for (int i = 0; i < m_cdb->m_numLeaves; i++) {
        for (int j = 0; j < leaf->m_count; j++) {
            offsetPtr(leaf->m_groups[j], (int)PTR_TO_ADDR(base));
        }
        leaf = (CDBLeafNode*)((u8*)leaf + leaf->m_count * 4 + 0x44);
    }

    CDBBranchNode* branch = m_cdb->m_branches;
    for (int i = 0; i < m_cdb->m_numBranches; i++) {
        offsetPtr(branch->m_front, (int)PTR_TO_ADDR(base));
        offsetPtr(branch->m_back, (int)PTR_TO_ADDR(base));
        branch++;
    }

    if (m_cdb->m_numLeaves > 0) {
        // Axis-aligned extents of the database's bounding box (the height
        // terms are computed in double precision, as in the original).
        const CDBBox& b = m_cdb->m_bounds;
        float ex = b.m_width * (0.5f * (float)fabs(b.m_basis[0][0])) +
                   b.m_depth * (0.5f * (float)fabs(b.m_basis[1][0])) + b.m_height * (0.5 * fabs(b.m_basis[2][0]));
        float ey = b.m_width * (0.5f * (float)fabs(b.m_basis[0][1])) +
                   b.m_depth * (0.5f * (float)fabs(b.m_basis[1][1])) + b.m_height * (0.5 * fabs(b.m_basis[2][1]));
        float ez = b.m_width * (0.5f * (float)fabs(b.m_basis[0][2])) +
                   b.m_depth * (0.5f * (float)fabs(b.m_basis[1][2])) + b.m_height * (0.5 * fabs(b.m_basis[2][2]));
        m_min.x = b.m_center[0] - ex;
        m_min.y = b.m_center[1] - ey;
        m_min.z = b.m_center[2] - ez;
        m_max.x = b.m_center[0] + ex;
        m_max.y = b.m_center[1] + ey;
        m_max.z = b.m_center[2] + ez;
    } else {
        m_min.z = 0.0f;
        m_min.y = 0.0f;
        m_min.x = 0.0f;
        m_max.z = 0.0f;
        m_max.y = 0.0f;
        m_max.x = 0.0f;
    }
}

void CCDBObject::DisablePassThru(bool disable) {
    g_bDisablePassThru = disable;
}

CCDBObject::CCDBObject(const CCDBObject& other) : m_cdb(other.m_cdb), m_min(other.m_min), m_max(other.m_max) {}

CCDBObject::CCDBObject(CDB* cdb) : m_cdb(cdb) {}

// Line against one group: only when the line crosses the group's plane;
// keeps the nearest hit along the line.
int TestCollisionWithTriGroup(const CDBTriGroup& group, const CLine3& line, CCollision& collision, bool contacts) {
    int result = CLSN_NONE;
    PROFILE_BEGIN(PROFBIN_CDB_TRIGROUP);
    CPlane plane(CVector3(group.m_normal[0], group.m_normal[1], group.m_normal[2]), group.m_d);
    CVector3 start = line.m_start;
    CVector3 end = line.m_end;
    if (((plane.m_normal[0] * start.x + plane.m_normal[1] * start.y + plane.m_normal[2] * start.z) - plane.m_d) *
            ((plane.m_normal[0] * end.x + plane.m_normal[1] * end.y + plane.m_normal[2] * end.z) - plane.m_d) <
        0.0f) {
        for (int i = 0; i < -group.m_count; i++) {
            const CDBTri* t = group.m_tris[i];
            CCDBTriangle tri(*t);
            CCollision hit(collision);
            PROFILE_BEGIN(PROFBIN_CDB_TRI);
            int triResult = tri.TestCollision(line, hit, t, contacts);
            PROFILE_END(PROFBIN_CDB_TRI);
            if (triResult != CLSN_NONE) {
                if (hit.m_fraction <= collision.m_fraction) {
                    collision = hit;
                    result = triResult;
                }
                if (result == CLSN_PENETRATING) {
                    PROFILE_END(PROFBIN_CDB_TRIGROUP);
                    return result;
                }
            }
        }
    }
    PROFILE_END(PROFBIN_CDB_TRIGROUP);
    return result;
}
