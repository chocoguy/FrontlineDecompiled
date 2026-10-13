// The AI filter layer. A filter sits between a game object's AI doodad and
// its CAIObject: once a frame it copies the game state in (position, axes,
// velocity, health), lets the AI run, and copies the result back (fire
// flags, snap moves, the point to walk to). CAIFilterGlobal holds the
// global AI services: game time, line-of-sight and collision lines against
// the world and mechanics, cover points and waypoints in the level's BSP
// leaves, the A* path nodes, the spline paths and sound propagation.
#include "game/AIFilter.h"
#include "game/AIObject.h"
#include "game/AIPlayerDoodad.h"
#include "game/AIPlayerObject.h"
#include "game/AISoldierDoodad.h"
#include "game/AISoldierObject.h"
#include "game/AISplinePath.h"
#include "game/AITankDoodad.h"
#include "game/AITankObject.h"
#include "game/BSGO_Basic.h"
#include "game/CoverPoint.h"
#include "game/ISceneNode.h"
#include "game/MathFun.h"
#include "game/Moh2.h"
#include "game/PathNodeList.h"
#include "game/SearchNode.h"
#include "game/Soldier_object.h"
#include "game/Tank_object.h"
#include "game/anim_obj.h"
#include "game/bsobject.h"
#include "game/bstypes.h"
#include "game/collision.h"
#include "game/fiber.h"
#include "game/line.h"
#include "game/matrix.h"
#include "game/player.h"
#include "game/propdat.h"
#include "game/scene.h"
#include "game/vector.h"
#include "game/weapon.h"
#include "math.h"

#define COVERPOINT_CRC 0x9101C906
#define TRIGGER_TYPE_WAYPOINT 5

#define AIEVT_SEES_PLAYER 0xE1  // an AI has the player in view (data: the player's target)

CAIFilterGlobal g_aigAIFilterGlobalObject;
float CAIFilterGlobal::s_rNextSoundPropagateTime;

// Recently propagated sounds: the same source is not propagated again
// within a second unless the new sound is of a higher type.
struct NoiseInfo {
    /* 0x0 */ void* m_source;
    /* 0x4 */ u16 m_type;
    /* 0x8 */ f32 m_time;
};

#define MAX_RECENT_SOUNDS 16
static NoiseInfo recent_sounds_mem[MAX_RECENT_SOUNDS];
static int s_numRecentSounds;

static inline void EraseRecentSound(NoiseInfo* n) {
    NoiseInfo* end = recent_sounds_mem + s_numRecentSounds;
    for (NoiseInfo* next = n + 1; next != end; n++, next++) {
        *n = *next;
    }
    s_numRecentSounds--;
}

// The pieces of a level's trigger list that are cover points (they double
// as CCoverPoint: flags word, then the data).
static inline bool IsCoverPoint(TriggerObject_struct* t) {
    TriggerObjectData_struct* data = t->m_data;
    return data->m_type == TRIGGER_TYPE_WAYPOINT && (u32)data->field_0x48 == COVERPOINT_CRC;
}

static inline TriggerObject_struct* TriggersEnd() {
    return g_pTriggerObjects + g_pNumTriggers;
}

// The first cover point from `t` on, or NULL.
static inline CCoverPoint* CoverFrom(TriggerObject_struct* t) {
    for (; t < TriggersEnd(); t++) {
        if (IsCoverPoint(t)) {
            return (CCoverPoint*)t;
        }
    }
    return NULL;
}

// Cover point iteration: the leaf's own list (from index `i`), then every
// cover point of the level.
static inline CCoverPoint* NextCover(const _PropBSPLeaf* leaf, int i, CCoverPoint* cur) {
    if (i < leaf->m_numList28) {
        return (CCoverPoint*)&g_pTriggerObjects[leaf->m_list28[i]];
    }
    if (i == leaf->m_numList28) {
        return CoverFrom(g_pTriggerObjects);
    }
    return CoverFrom((TriggerObject_struct*)cur + 1);
}

static inline TriggerObjectData_struct* CoverData(CCoverPoint* c) {
    return ((TriggerObject_struct*)c)->m_data;
}

static inline u8 AITeam(CAIObject* ai) {
    if (ai != NULL) {
        return ai->field_0x14;
    }
    return 0;
}

static inline BSObject* ScriptOf(CAIObject* ai) {
    return ((CAIFilterSoldierObject*)ai->m_pFilter)->m_pBSObject;
}

// What a filter's game object is (each doodad keeps it at 0x50).
static inline CSoldierObject* DoodadSoldier(CAIDoodad* doodad) {
    return ((CAISoldierDoodad*)doodad)->m_pSoldier;
}

static inline CTankObject* DoodadTank(CAIDoodad* doodad) {
    return ((CAITankDoodad*)doodad)->m_pTank;
}

static inline ISceneNode* DoodadNode(CAIDoodad* doodad) {
    return (ISceneNode*)((CAISoldierDoodad*)doodad)->m_pSoldier;
}

static inline int ScriptHealth(BSGO_Basic* go) {
    return *(int*)((u8*)go + 0x10);
}

// Position, axes and velocity of the AI object from its scene node.
static inline void CopyMotion(CAIObject* ai) {
    ai->m_velocity.field_0x0 = ai->m_position.field_0x0 - ai->m_prevPosition.field_0x0;
    ai->m_velocity.field_0x4 = ai->m_position.field_0x4 - ai->m_prevPosition.field_0x4;
    ai->m_velocity.field_0x8 = ai->m_position.field_0x8 - ai->m_prevPosition.field_0x8;
    ai->m_prevPosition.field_0x0 = ai->m_position.field_0x0;
    ai->m_prevPosition.field_0x4 = ai->m_position.field_0x4;
    ai->m_prevPosition.field_0x8 = ai->m_position.field_0x8;
    float vx = ai->m_velocity.field_0x0;
    float vy = ai->m_velocity.field_0x4;
    float vz = ai->m_velocity.field_0x8;
    ai->m_speed = sqrtf(vz * vz + (vx * vx + vy * vy));
}

void CAIFilterSoldierObject::GetLookDirection(CAIFilterRealVector3& look, CAIFilterRealVector3& eye) {
    CAIDoodad* doodad = m_pDoodad;
    CMatrix m;
    DoodadSoldier(doodad)->GetLookMatrix(m);
    CAIFilterRealVector3 v = *(CAIFilterRealVector3*)m.m[1];
    look.field_0x0 = v.field_0x0;
    look.field_0x4 = v.field_0x4;
    look.field_0x8 = v.field_0x8;
    v = *(CAIFilterRealVector3*)m.m[3];
    eye.field_0x0 = v.field_0x0;
    eye.field_0x4 = v.field_0x4;
    eye.field_0x8 = v.field_0x8;
}

void CAIFilterSoldierObject::SetMoveTarget(const CAIFilterRealPosition& pos) {
    DoodadSoldier(m_pDoodad)->SetMoveTarget(*(const CVector3*)&pos);
}

void CAIFilterTankObject::GetLookDirection(CAIFilterRealVector3& look, CAIFilterRealVector3& eye) {
    CAIDoodad* doodad = m_pDoodad;
    CMatrix m;
    DoodadTank(doodad)->GetLookMatrix(m);
    CAIFilterRealVector3 v = *(CAIFilterRealVector3*)m.m[1];
    look.field_0x0 = v.field_0x0;
    look.field_0x4 = v.field_0x4;
    look.field_0x8 = v.field_0x8;
    v = *(CAIFilterRealVector3*)m.m[3];
    eye.field_0x0 = v.field_0x0;
    eye.field_0x4 = v.field_0x4;
    eye.field_0x8 = v.field_0x8;
}

CAIFilterGlobal::~CAIFilterGlobal() {}

void CAIFilterGlobal::UpdateFromGlobals(float frames) {
    m_frame = GetFrameCount();
    m_time += 0.016666668f * frames;
}

void CAIFilterGlobal::UpdateToGlobals() {}

// Tells the AIs about a sound, unless the same source made one of at least
// this type within the last second. At most 15 are remembered.
void CAIFilterGlobal::PropagateSoundToAllObjects(const CAIFilterSoundInfo& info) {
    NoiseInfo noise;
    noise.m_source = info.m_source;
    noise.m_type = info.m_type;
    noise.m_time = g_aigAIFilterGlobalObject.m_time;
    NoiseInfo* n = recent_sounds_mem;
    while (n != recent_sounds_mem + s_numRecentSounds) {
        if (g_aigAIFilterGlobalObject.m_time - n->m_time > 1.0f) {
            EraseRecentSound(n);
        } else {
            n++;
        }
    }
    NoiseInfo* end = recent_sounds_mem + s_numRecentSounds;
    bool skip = false;
    for (n = recent_sounds_mem; n != end; n++) {
        if (n->m_source == noise.m_source) {
            break;
        }
    }
    if (n != end) {
        if (noise.m_type <= n->m_type) {
            skip = true;
        } else {
            EraseRecentSound(n);
        }
    }
    if (!skip) {
        while (s_numRecentSounds >= 15) {
            NoiseInfo* oldest = recent_sounds_mem;
            NoiseInfo* last = recent_sounds_mem + s_numRecentSounds;
            for (NoiseInfo* m = oldest + 1; m != last; m++) {
                if (m->m_time < oldest->m_time) {
                    oldest = m;
                }
            }
            EraseRecentSound(oldest);
        }
        CAIObject::PropagateSoundToAllObjects(info, CAIObject::GetGlobalListStart());
        recent_sounds_mem[s_numRecentSounds++] = noise;
        s_rNextSoundPropagateTime = 1.0f + g_aigAIFilterGlobalObject.m_time;
    }
}

static inline void SwapWord(void* p) {
    u8* b = (u8*)p;
    u8 t = b[0];
    b[0] = b[3];
    b[3] = t;
    t = b[1];
    b[1] = b[2];
    b[2] = t;
}

// Builds the spline paths from the level's poly paths (`base` is the file
// the point offsets are relative to).
void CAIFilterGlobal::CreateSplinePathManager(void* base, BPDPolyPath* paths, int count) {
    if (count != 0) {
        m_splinePaths = new CAISplinePathManager(count);
        m_splinePaths->AllocateGenerateBuffers();
        for (int i = 0; i < count; i++, paths++) {
            paths->m_points = (_aifilter_simple_v3*)((u8*)paths->m_points + (u32)base);
            int j;
            for (j = 0; j < paths->m_numPoints; j++) {
                _aifilter_simple_v3* pt = &paths->m_points[j];
                SwapWord(&pt->x);
                SwapWord(&pt->y);
                SwapWord(&pt->z);
            }
            m_splinePaths->GenerateSplinePath(i, paths->m_id, paths->m_numPoints, paths->m_points);
        }
        m_splinePaths->FreeGenerateBuffers();
    }
}

// On a level restart the paths are kept; the searches, sounds and AI list
// are reset.
void CAIFilterGlobal::ResetSplinePathManager(void*, BPDPolyPath*, int) {
    CSearchNode::Reset();
    s_numRecentSounds = 0;
    CAIObject::ResetGlobalList();
}

// The path with this poly path id (one past the last if there is none).
CAISplinePath* CAIFilterGlobal::GetSplinePath(unsigned long id, bool reverse) const {
    if (reverse) {
        id += 0x01000000;
    }
    CAISplinePathManager* mgr = m_splinePaths;
    u32 i;
    for (i = 0; i < mgr->m_numPaths; i++) {
        if (id == mgr->m_paths[i].m_id) {
            break;
        }
    }
    return &mgr->m_paths[i];
}

void CAIFilterGlobal::CreateAStarPathNodeList(void* base, BPDPathFindingNode* nodes, int count) {
    if (count != 0) {
        m_pathNodes = new CPathNodeList(base, nodes, count);
    }
}

CPathNode* CAIFilterGlobal::GetAStarPathNodeByIndex(int index) const {
    CPathNode* node = NULL;
    if (m_pathNodes != NULL) {
        node = m_pathNodes->GetPathNodeByID(index);
    }
    return node;
}

CPathNode* CAIFilterGlobal::GetClosestAStarPathNode(const CAIFilterRealPosition& pos,
                                                    const CAIFilterRealVector3& dir) const {
    return m_pathNodes->GetClosestNode(pos, dir);
}

void CAIFilterGlobal::EnableAllAStarPathNodes() {
    m_pathNodes->EnableAll();
}

void CAIFilterGlobal::DisableCollidingAStarPathNodes(const IVolume* volume) {
    m_pathNodes->DisableCollidingNodes(volume);
}

// Invalidates the cover points with this placed id.
void CAIFilterGlobal::InvalidateCoverPointByID(int id) {
    for (CCoverPoint* c = CoverFrom(g_pTriggerObjects); c != NULL; c = CoverFrom((TriggerObject_struct*)c + 1)) {
        if (CoverData(c)->field_0x38 == id) {
            c->Invalidate();
        }
    }
}

CCoverPoint* CAIFilterGlobal::GetCoverPointByIndex(int index) const {
    return (CCoverPoint*)&g_pTriggerObjects[index];
}

int CAIFilterGlobal::GetCoverPointIndex(CCoverPoint* cover) const {
    return (TriggerObject_struct*)cover - g_pTriggerObjects;
}

// The closest (in XY, within sqrt(maxDistSq)) free cover point of our team
// (or none) that covers from `threat`; with `walkMode`, also one that can
// be walked to from `pos` (its peek type and approach point are returned).
CCoverPoint* CAIFilterGlobal::GetClosestUsableCoverPoint(const _PropBSPLeaf* leaf, const CAIFilterRealPosition& pos,
                                                         const CAIFilterRealPosition* threat,
                                                         COVERPOINT_WALK_MODE* walkMode, COVERPOINT_TYPE* type,
                                                         CAIFilterRealPosition* approach, float maxDistSq,
                                                         CAIObject* ai) const {
    u8 team = AITeam(ai);
    int i = 0;
    // (The original starts with the level's first cover point, then goes on
    // from the leaf's second entry.)
    CCoverPoint* c = CoverFrom(g_pTriggerObjects);
    float bestDist = maxDistSq;
    CCoverPoint* best = NULL;
    while (c != NULL) {
        COVERPOINT_TYPE peek = c->GetAvailablePeekPosition();
        u8 coverTeam = CoverData(c)->m_team;
        if (peek != 0 && (coverTeam == 0 || coverTeam == team)) {
            CAIFilterRealVector3 spot;
            c->GetModifiedPosition(spot, peek);
            float dy = spot.field_0x4 - pos.field_0x4;
            float dx = spot.field_0x0 - pos.field_0x0;
            float d = dx * dx + dy * dy;
            if (d < bestDist) {
                bool covers;
                if (threat != NULL) {
                    covers = c->ProvidesCover(*(CAIFilterRealPosition*)&spot, threat, NULL);
                } else {
                    covers = true;
                }
                if (covers) {
                    if (walkMode != NULL) {
                        int mode = c->CheckLineCollision(*(CAIFilterRealPosition*)&spot, pos, approach);
                        if (mode != 0) {
                            *walkMode = (COVERPOINT_WALK_MODE)mode;
                            bestDist = d;
                            best = c;
                            *type = peek;
                        }
                    } else {
                        bestDist = d;
                        best = c;
                    }
                }
            }
        }
        i++;
        c = NextCover(leaf, i, c);
    }
    return best;
}

// The closest cover point (within sqrt(maxDistSq), nearer to us than to
// the threat and within sqrt(maxThreatDistSq) of it) placed relative to
// the threat as `type` asks, that covers from it.
CCoverPoint* CAIFilterGlobal::ChooseCoverPointByType(const _PropBSPLeaf* leaf, const CAIFilterRealPosition& pos,
                                                     const CAIFilterRealPosition* threat, ECoverPointSelectionType type,
                                                     float maxDistSq, float maxThreatDistSq, CAIObject* ai) const {
    u8 team = AITeam(ai);
    float ty = threat->field_0x4 - pos.field_0x4;
    float tx = threat->field_0x0 - pos.field_0x0;
    float tz = threat->field_0x8 - pos.field_0x8;
    float threatDistSq = tz * tz + (tx * tx + ty * ty);
    if (threatDistSq < 36.0f) {
        return NULL;
    }
    float len = sqrtf(threatDistSq);
    if (len != 0.0f) {
        float inv = 1.0f / len;
        tx *= inv;
        ty *= inv;
        tz *= inv;
    }
    int i = 0;
    CCoverPoint* c;
    if (i < leaf->m_numList28) {
        c = (CCoverPoint*)&g_pTriggerObjects[leaf->m_list28[0]];
    } else {
        c = CoverFrom(g_pTriggerObjects);
    }
    float bestDist = maxDistSq;
    CCoverPoint* best = NULL;
    while (c != NULL) {
        COVERPOINT_TYPE peek = c->GetAvailablePeekPosition();
        u8 coverTeam = CoverData(c)->m_team;
        if (peek != 0 && (coverTeam == 0 || coverTeam == team)) {
            CAIFilterRealVector3 spot;
            c->GetModifiedPosition(spot, peek);
            float ay = threat->field_0x4 - spot.field_0x4;
            float ax = threat->field_0x0 - spot.field_0x0;
            float az = threat->field_0x8 - spot.field_0x8;
            float fromThreat = az * az + (ax * ax + ay * ay);
            if (fromThreat < maxThreatDistSq) {
                float by = pos.field_0x4 - spot.field_0x4;
                float bx = pos.field_0x0 - spot.field_0x0;
                float bz = pos.field_0x8 - spot.field_0x8;
                float fromUs = bz * bz + (bx * bx + by * by);
                if (fromUs < bestDist && fromUs < fromThreat) {
                    float dy = spot.field_0x4 - pos.field_0x4;
                    float dx = spot.field_0x0 - pos.field_0x0;
                    float dz = spot.field_0x8 - pos.field_0x8;
                    float dlen = dz * dz + (dx * dx + dy * dy);
                    if (dlen > 0.0f) {
                        dlen = sqrtf(dlen);
                    }
                    if (dlen != 0.0f) {
                        float inv = 1.0f / dlen;
                        dx *= inv;
                        dy *= inv;
                        dz *= inv;
                    }
                    // Where the spot is relative to the line to the threat.
                    float along = tz * dz + (tx * dx + ty * dy);
                    bool ok;
                    switch (type) {
                    case 0:
                        ok = false;
                        if (along > 0.38f && fromThreat < fromUs && fromUs < threatDistSq) {
                            ok = true;
                        }
                        break;
                    case 1:
                        ok = false;
                        if (along > 0.38f && fromUs < fromThreat && fromUs < threatDistSq) {
                            ok = true;
                        }
                        break;
                    case 2: {
                        bool r = false;
                        bool side = false;
                        if (fabs(along) < (double)0.38f && fromUs < fromThreat) {
                            side = true;
                        }
                        if (side && threatDistSq < fromUs) {
                            r = true;
                        }
                        ok = r;
                        break;
                    }
                    case 3: {
                        bool r = false;
                        bool side = false;
                        if (fabs(along) < (double)0.38f && fromUs < fromThreat) {
                            side = true;
                        }
                        if (side && fromUs < threatDistSq) {
                            r = true;
                        }
                        ok = r;
                        break;
                    }
                    case 5:
                        ok = false;
                        if (along < -0.38f && fromUs < fromThreat && fromUs < threatDistSq) {
                            ok = true;
                        }
                        break;
                    case 4:
                        ok = false;
                        if (along < -0.38f && fromUs < fromThreat && threatDistSq < fromUs) {
                            ok = true;
                        }
                        break;
                    case 6:
                        ok = true;
                        break;
                    default:
                        ok = false;
                        break;
                    }
                    if (ok) {
                        bool covers;
                        if (threat != NULL) {
                            covers = c->ProvidesCover(*(CAIFilterRealPosition*)&spot, threat, ai);
                        } else {
                            covers = true;
                        }
                        if (covers) {
                            bestDist = fromUs;
                            best = c;
                        }
                    }
                }
            }
        }
        i++;
        c = NextCover(leaf, i, c);
    }
    return best;
}

// A path node leading away from `threat`, within `dist`.
bool CAIFilterObject::FindRunAwayPoint(const ISceneNode* threat, float dist, CAIFilterRealPosition* out) {
    CAIObject* ai = m_pAIObject;
    CVector3 tp;
    threat->GetPosition(tp);
    CAIFilterRealPosition from;
    from.field_0x0 = tp.x;
    from.field_0x4 = tp.y;
    from.field_0x8 = tp.z;
    CPathNode* node =
        g_aigAIFilterGlobalObject.m_pathNodes->GreedySearchAwayFromPoint(ai->m_position, from, dist);
    if (node != NULL) {
        out->field_0x0 = node->m_pos[0];
        out->field_0x4 = node->m_pos[1];
        out->field_0x8 = node->m_pos[2];
        return true;
    }
    return false;
}

static inline bool IsWaypointOfType(TriggerObject_struct* t, DWI_OBJECT_CRC_ENUM type) {
    TriggerObjectData_struct* data = t->m_data;
    return data->m_type == TRIGGER_TYPE_WAYPOINT && data->field_0x48 == (s32)type;
}

static inline TriggerObject_struct* WaypointFrom(TriggerObject_struct* t, DWI_OBJECT_CRC_ENUM type) {
    for (; t < TriggersEnd(); t++) {
        if (IsWaypointOfType(t, type)) {
            return t;
        }
    }
    return NULL;
}

// Waypoint iteration: those of the leaf's list with the CRC (from index
// `*i`), then (with `all`) every waypoint of the level of that kind.
static inline TriggerObject_struct* NextWaypoint(const _PropBSPLeaf* leaf, int* i, TriggerObject_struct* cur,
                                                 DWI_OBJECT_CRC_ENUM type, bool all) {
    int n = leaf->m_numList24;
    for (; *i < n; (*i)++) {
        TriggerObject_struct* t = &g_pTriggerObjects[leaf->m_list24[*i]];
        if (t->m_data->field_0x48 == (s32)type) {
            return t;
        }
    }
    if (*i == n) {
        if (all) {
            return WaypointFrom(g_pTriggerObjects, type);
        }
        return NULL;
    }
    return WaypointFrom(cur + 1, type);
}

// The closest waypoint of `type` (in XY, at about our height) within
// sqrt(maxDistSq).
TriggerObject_struct* CAIFilterGlobal::GetClosestWaypointByType(const _PropBSPLeaf* leaf,
                                                                const CAIFilterRealPosition& pos,
                                                                DWI_OBJECT_CRC_ENUM type, bool all,
                                                                float maxDistSq) const {
    int i = 0;
    TriggerObject_struct* t = NextWaypoint(leaf, &i, NULL, type, all);
    float bestDist = maxDistSq;
    TriggerObject_struct* best = NULL;
    while (t != NULL) {
        TriggerObjectData_struct* data = t->m_data;
        f32 wp[3];
        wp[2] = data->m_position[2];
        wp[1] = data->m_position[1];
        wp[0] = data->m_position[0];
        if (MathFunCloseToZero(wp[2] - pos.field_0x8, 2.0f)) {
            float dy = wp[1] - pos.field_0x4;
            float dx = wp[0] - pos.field_0x0;
            float d = dx * dx + dy * dy;
            if (d < bestDist) {
                bestDist = d;
                best = t;
            }
        }
        i++;
        t = NextWaypoint(leaf, &i, t, type, all);
    }
    return best;
}

bool MechanicCollisionChecker::Visit(ISceneNode& node) {
    if (&node == m_ignore) {
        return false;
    }
    CCollision collision;
    int id = node.GetCollisionId();
    bool test = false;
    if (id != -1 && id != 16 && id != 22) {
        test = true;
    }
    if (m_skipSome) {
        bool ok = false;
        if (id != 15 && id != 17) {
            ok = true;
        }
        test &= ok;
    }
    if (test) {
        const IVolume* volume = node.GetWorldBoundingVolume((ISceneNode::EVolumeType)0);
        if (volume != NULL && volume->TestCollision(m_fiber, collision, false) != 1) {
            if (m_length > 0.0f) {
                float d = m_length * collision.m_fraction;
                if (d < m_closest) {
                    m_closest = d;
                }
            }
            return m_stopOnHit;
        }
    }
    return false;
}

// The segment from `a` to `b` as a CLine3 (filled in place, as the
// original's inline constructor did).
struct AILine {
    f64 m_storage[8];
    CLine3& Line() { return *(CLine3*)m_storage; }
    AILine(const CAIFilterRealPosition& a, const CAIFilterRealPosition& b) {
        f32* f = (f32*)m_storage;
        f[0] = a.field_0x0;
        f[1] = a.field_0x4;
        f[2] = a.field_0x8;
        f[4] = b.field_0x0;
        f[5] = b.field_0x4;
        f[6] = b.field_0x8;
        f[8] = f[4] - f[0];
        f[9] = f[5] - f[1];
        f[10] = f[6] - f[2];
        ((u8*)m_storage)[0x38] = 0;
        ((u8*)m_storage)[0x39] = 0;
    }
};

static inline ISceneNode* IgnoredNode(CAIFilterObject* filter) {
    ISceneNode* ignore = NULL;
    if (filter != NULL) {
        ignore = ((CAIFilterSoldierObject*)filter)->m_pBSObject->m_gameObject->GetSceneNode();
    }
    return ignore;
}

// Whether the line from `from` to `to` is blocked for seeing (some
// collision ids do not block sight). True when blocked.
int CAIFilterGlobal::CheckVisionLineToEnvironment(const CAIFilterRealPosition& from, const CAIFilterRealPosition& to,
                                                  CAIFilterObject* filter) {
    int result = 1;
    ISceneNode* world = (ISceneNode*)g_scene.m_world;
    AILine line(from, to);
    CCollision collision;
    CVolFiber fiber(line.Line());
    const IVolume* volume = world->GetWorldBoundingVolume((ISceneNode::EVolumeType)0);
    if (volume != NULL) {
        collision.m_flags &= ~0x80;
        result = volume->TestCollision(line.Line(), collision, false);
    }
    int blocked = result != 1;
    if (!blocked) {
        MechanicCollisionChecker checker(fiber, true, true, -1.0f, IgnoredNode(filter));
        blocked = g_scene.VisitAllStaticObjects(checker);
    }
    return blocked;
}

// Whether the line from `from` to `to` collides with the world or a
// mechanic. True when blocked.
bool CAIFilterGlobal::CheckCollisionLineToEnvironment(const CAIFilterRealPosition& from,
                                                      const CAIFilterRealPosition& to, CAIFilterObject* filter) {
    int result = 1;
    ISceneNode* world = (ISceneNode*)g_scene.m_world;
    AILine line(from, to);
    CCollision collision;
    CVolFiber fiber(line.Line());
    const IVolume* volume = world->GetWorldBoundingVolume((ISceneNode::EVolumeType)0);
    if (volume != NULL) {
        result = volume->TestCollision(fiber, collision, false);
    }
    int blocked = result != 1;
    if (!blocked) {
        MechanicCollisionChecker checker(fiber, true, false, -1.0f, IgnoredNode(filter));
        blocked = g_scene.VisitAllStaticObjects(checker);
    }
    return blocked;
}

MechanicCollisionChecker::~MechanicCollisionChecker() {}

// The distance along the line (scaled by `length`) to the first hit, or -1.
float CAIFilterGlobal::GetCollisionDistLineToEnvironment(const CAIFilterRealPosition& from,
                                                         const CAIFilterRealPosition& to, CAIFilterObject* filter,
                                                         float length) {
    AILine line(from, to);
    CCollision collision;
    ISceneNode* world = (ISceneNode*)g_scene.m_world;
    int result = 1;
    CVolFiber fiber(line.Line());
    const IVolume* volume = world->GetWorldBoundingVolume((ISceneNode::EVolumeType)0);
    if (volume != NULL) {
        result = volume->TestCollision(fiber, collision, true);
    }
    bool hit = result != 1;
    if (hit) {
        length *= collision.m_fraction;
    }
    MechanicCollisionChecker checker(fiber, false, false, length, IgnoredNode(filter));
    g_scene.VisitAllStaticObjects(checker);
    if (checker.m_closest < length) {
        length = checker.m_closest;
        hit = true;
    }
    if (hit) {
        return length;
    }
    return -1.0f;
}

CAIFilterObject::~CAIFilterObject() {}

BSObject* CAIFilterObject::ChooseTarget(float) {
    return NULL;
}

void CAIFilterObject::SetTargetMode(aifilter_target_mode) {}

void CAIFilterObject::ResetTargetMatchList(aifilter_target_mode) {}

void CAIFilterObject::SetupTargetMatchList(aifilter_target_mode, aistatus_match*, int) {}

void CAIFilterObject::LinkGameObject(void* doodad) {
    m_pDoodad = (CAIDoodad*)doodad;
}

BSObject* CAIFilterObject::GetTargetScript() const {
    CAIObject* target = m_pAIObject->m_pTarget;
    if (target != NULL) {
        return ScriptOf(target);
    }
    return NULL;
}

// The AI object lives right after the filter, 16-byte aligned.
static inline void* AIObjectBuffer(void* filter, u32 size) {
    u8* p = (u8*)filter + size;
    if ((u32)p & 0xF) {
        p = (u8*)(((u32)p & ~0xF) + 0x10);
    }
    return p;
}

CAIFilterSoldierObject::CAIFilterSoldierObject() {
    m_pAIObject = new (AIObjectBuffer(this, sizeof(CAIFilterSoldierObject))) CAISoldierObject(this);
}

CAIFilterSoldierObject::~CAIFilterSoldierObject() {
    delete m_pAIObject;
}

void* CAIFilterSoldierObject::operator new(unsigned long, void* buffer) {
    return buffer;
}

// Aim, vision and hearing settings from the soldier's placed object.
void CAIFilterSoldierObject::SetOneTimeVars() {
    CAIDoodad* doodad = m_pDoodad;
    CAISoldierObject* ai = (CAISoldierObject*)m_pAIObject;
    m_pBSObject = doodad->GetSceneNode()->GetScriptObject();
    m_pGameObject = m_pBSObject->m_gameObject;
    TriggerObjectData_struct* data = ((ISceneNode*)DoodadSoldier(doodad))->GetScriptObject()->m_trigger->m_data;
    u8 delay = data->field_0x79;
    if (delay == 0) {
        ai->field_0x1F0 = 0.1f;
    } else {
        ai->field_0x1F0 = 0.1f * delay;
    }
    switch (data->m_ai.m_aimTime) {
    case 0:
        ai->m_aimTime = 0.5f;
        break;
    case 1:
        ai->m_aimTime = 1.0f;
        break;
    case 2:
        ai->m_aimTime = 2.0f;
        break;
    }
    switch (data->m_aiSkill) {
    case 0:
        ai->field_0x1F4 = 0.05235988f;  // 3 degrees
        ai->m_inaccuracy = 1.25f;
        break;
    case 1:
        ai->field_0x1F4 = 0.034906585f;
        ai->m_inaccuracy = 0.75f;
        break;
    case 2:
        ai->field_0x1F4 = 0.017453292f;
        ai->m_inaccuracy = 0.333f;
        break;
    }
    ai->field_0x204 = 0.0625f * data->field_0x7E;
    ai->m_sightRange = 0.0625f * data->field_0x80;
    ai->m_hearingRadius = 0.0625f * data->field_0x82;
    ai->field_0x214 = data->field_0x72;
    ai->field_0x14 = data->m_team;
    ai->field_0x15 = 4;
    ai->m_placedId = data->field_0x38;
    ai->m_maxHealth = data->field_0x70;
    ai->m_health = ai->m_maxHealth;
    UpdateFromGameObject();
}

// The current weapon's bullet speed and the matching aim height mode.
void CAIFilterSoldierObject::UpdateManualUpdateVars() {
    float speed = 1.0f;
    CAIDoodad* doodad = m_pDoodad;
    CAIObject* ai = m_pAIObject;
    CSoldierObject* soldier = DoodadSoldier(doodad);
    CWeapon* weapon = soldier->m_pWeapons[soldier->m_curWeapon];
    if (weapon != NULL) {
        speed = weapon->GetBulletSpeed();
    }
    ai->m_bulletSpeed = speed;
    ai->m_invBulletSpeed = 1.0f / ai->m_bulletSpeed;
    soldier = DoodadSoldier(doodad);
    weapon = soldier->m_pWeapons[soldier->m_curWeapon];
    int mode;
    if (weapon == NULL) {
        mode = 9;
    } else {
        switch (weapon->m_weaponType) {
        default:
            mode = 1;
            break;
        case 10:
        case 11:
        case 12:
        case 13:
        case 28:
            mode = 2;
            break;
        case 30:
        case 31:
        case 32:
            mode = 5;
            break;
        case 17:
        case 18:
        case 19:
        case 22:
        case 23:
            mode = 4;
            break;
        case 33:
        case 34:
        case 40:
            mode = 6;
            break;
        case 38:
            mode = 8;
            break;
        }
    }
    ai->m_aimHeightMode = mode;
}

BSObject* CAIFilterSoldierObject::ChooseTarget(float range) {
    CAISoldierObject* ai = (CAISoldierObject*)m_pAIObject;
    CAIObject* target = ai->ChooseTarget(CAIObject::GetGlobalListStart(), range);
    if (target != NULL) {
        return ScriptOf(target);
    }
    return NULL;
}

void CAIFilterSoldierObject::SetTargetMode(aifilter_target_mode mode) {
    CAISoldierObject* ai = (CAISoldierObject*)m_pAIObject;
    ai->m_targetMode = mode;
    ai->ResetTargetMatchList(mode);
}

void CAIFilterSoldierObject::ResetTargetMatchList(aifilter_target_mode mode) {
    ((CAISoldierObject*)m_pAIObject)->ResetTargetMatchList(mode);
}

void CAIFilterSoldierObject::SetupTargetMatchList(aifilter_target_mode mode, aistatus_match* list, int count) {
    ((CAISoldierObject*)m_pAIObject)->SetupTargetMatchList(mode, list, count);
}

void CAIFilterSoldierObject::PreUpdate() {
    UpdateFromGameObject();
    m_pAIObject->PreUpdate();
}

void CAIFilterSoldierObject::Update(float dt) {
    CAIObject* ai = m_pAIObject;
    ai->Update(dt);
    UpdateToGameObject(dt);
    ai->field_0xC = g_aigAIFilterGlobalObject.m_time;
}

void CAIFilterSoldierObject::UpdateFromGameObject() {
    m_pAIObject->field_0x1AC = m_pAIObject->field_0x1A8;
    m_pAIObject->field_0x1A8 &= 3;
    CAIDoodad* doodad = m_pDoodad;
    CAISoldierObject* ai = (CAISoldierObject*)m_pAIObject;
    CAIFilterRealPosition pos;
    ((ISceneNode*)DoodadSoldier(doodad))->GetPosition(*(CVector3*)&pos);
    ai->SetLocation(pos);
    CAIFilterRealVector3 right;
    CAIFilterRealVector3 forward;
    CAIFilterRealVector3 up;
    ((ISceneNode*)DoodadSoldier(doodad))->GetRightward(*(CVector3*)&right);
    ((ISceneNode*)DoodadSoldier(doodad))->GetForward(*(CVector3*)&forward);
    ((ISceneNode*)DoodadSoldier(doodad))->GetUpward(*(CVector3*)&up);
    ai->SetCoordinateAxisDirection(right, forward, up);
    CopyMotion(ai);
    CSoldierObject* soldier = DoodadSoldier(doodad);
    pos.field_0x0 = soldier->m_upperTM.m[3][0];
    pos.field_0x4 = soldier->m_upperTM.m[3][1];
    pos.field_0x8 = soldier->m_upperTM.m[3][2];
    ai->SetUpperLocation(pos);
    // (The original fetches the axes again here, unused.)
    ((ISceneNode*)DoodadSoldier(doodad))->GetRightward(*(CVector3*)&right);
    ((ISceneNode*)DoodadSoldier(doodad))->GetForward(*(CVector3*)&forward);
    ((ISceneNode*)DoodadSoldier(doodad))->GetUpward(*(CVector3*)&up);
    ai->m_health = ScriptHealth(m_pGameObject);
    soldier = DoodadSoldier(doodad);
    ai->m_headPosition[0] = soldier->m_headPosition[0];
    ai->m_headPosition[1] = soldier->m_headPosition[1];
    ai->m_headPosition[2] = soldier->m_headPosition[2];
}

// Fire flags back to the doodad; a requested snap moves (and turns) the
// soldier; the walk target or aim point to the animation.
void CAIFilterSoldierObject::UpdateToGameObject(float dt) {
    m_pDoodad->m_fireFlags = m_pAIObject->field_0x1A8;
    m_pDoodad->m_prevFireFlags = m_pAIObject->field_0x1AC;
    CAIObject* ai = m_pAIObject;
    CAIDoodad* doodad = m_pDoodad;
    if (ai->field_0x1A8 & 2) {
        ai->field_0x1A8 &= ~2;
        ai->m_position.field_0x0 = ai->m_snapPosition.field_0x0;
        ai->m_position.field_0x4 = ai->m_snapPosition.field_0x4;
        ai->m_position.field_0x8 = ai->m_snapPosition.field_0x8;
        CVector3 cur;
        ((ISceneNode*)DoodadSoldier(doodad))->GetPosition(cur);
        ai->m_position.field_0x8 = cur.z;
        float inv = 1.0f / dt;
        cur.x = ai->m_position.field_0x0 - cur.x;
        cur.y = ai->m_position.field_0x4 - cur.y;
        cur.z = ai->m_position.field_0x8 - cur.z;
        cur.x *= inv;
        cur.y *= inv;
        cur.z *= inv;
        DoodadSoldier(doodad)->m_velocity.Copy(cur);
        if (ai->m_snapToAngle) {
            DoodadSoldier(doodad)->SetYaw(ai->m_snapAngle + DoodadSoldier(doodad)->GetYaw());
        }
    }
    if (ai->m_moveMode != 0 && ai->m_moveMode != 5) {
        if (ai->m_avoidObstacles) {
            DoodadSoldier(doodad)->SetMoveTarget(*(CVector3*)&ai->m_avoidPosition);
        } else {
            DoodadSoldier(doodad)->SetMoveTarget(*(CVector3*)&ai->m_destination);
        }
    } else if ((ai->m_flags & 2) == 2) {
        DoodadSoldier(doodad)->SetMoveTarget(*(CVector3*)&ai->m_targetPosition);
    }
}

CVector3 CAnimObject::GetWorldLinearVelocity() const {
    return m_velocity;
}

CAIFilterTankObject::CAIFilterTankObject() {
    m_pAIObject = new (AIObjectBuffer(this, sizeof(CAIFilterTankObject))) CAITankObject(this);
}

CAIFilterTankObject::~CAIFilterTankObject() {
    delete m_pAIObject;
}

void* CAIFilterTankObject::operator new(unsigned long, void* buffer) {
    return buffer;
}

void CAIFilterTankObject::SetOneTimeVars() {
    CAIDoodad* doodad = m_pDoodad;
    CAITankObject* ai = (CAITankObject*)m_pAIObject;
    m_pBSObject = doodad->GetSceneNode()->GetScriptObject();
    m_pGameObject = m_pBSObject->m_gameObject;
    TriggerObjectData_struct* data = ((ISceneNode*)DoodadTank(doodad))->GetScriptObject()->m_trigger->m_data;
    u8 delay = data->field_0x79;
    if (delay == 0) {
        ai->field_0x1F0 = 0.1f;
    } else {
        ai->field_0x1F0 = 0.1f * delay;
    }
    switch (data->m_ai.m_aimTime) {
    case 0:
        ai->m_aimTime = 0.5f;
        break;
    case 1:
        ai->m_aimTime = 1.0f;
        break;
    case 2:
        ai->m_aimTime = 2.0f;
        break;
    }
    switch (data->m_aiSkill) {
    case 0:
        ai->field_0x1F4 = 0.05235988f;
        ai->m_inaccuracy = 1.25f;
        break;
    case 1:
        ai->field_0x1F4 = 0.034906585f;
        ai->m_inaccuracy = 0.75f;
        break;
    case 2:
        ai->field_0x1F4 = 0.017453292f;
        ai->m_inaccuracy = 0.333f;
        break;
    }
    ai->field_0x204 = 0.0625f * data->field_0x7E;
    ai->m_sightRange = 0.0625f * data->field_0x80;
    ai->m_hearingRadius = 0.0625f * data->field_0x82;
    ai->field_0x214 = data->field_0x72;
    ai->field_0x14 = data->m_team;
    ai->field_0x15 = 7;
    ai->m_placedId = data->field_0x38;
    ai->m_maxHealth = data->field_0x70;
    ai->m_health = ai->m_maxHealth;
    ai->m_aimHeightMode = 7;
    UpdateFromGameObject();
}

void CAIFilterTankObject::UpdateManualUpdateVars() {}

BSObject* CAIFilterTankObject::ChooseTarget(float range) {
    CAITankObject* ai = (CAITankObject*)m_pAIObject;
    CAIObject* target = ai->ChooseTarget(CAIObject::GetGlobalListStart(), range);
    if (target != NULL) {
        return ScriptOf(target);
    }
    return NULL;
}

void CAIFilterTankObject::SetTargetMode(aifilter_target_mode mode) {
    ((CAITankObject*)m_pAIObject)->m_targetMode = mode;
}

void CAIFilterTankObject::ResetTargetMatchList(aifilter_target_mode mode) {
    ((CAITankObject*)m_pAIObject)->ResetTargetMatchList(mode);
}

void CAIFilterTankObject::SetupTargetMatchList(aifilter_target_mode mode, aistatus_match* list, int count) {
    ((CAITankObject*)m_pAIObject)->SetupTargetMatchList(mode, list, count);
}

void CAIFilterTankObject::PreUpdate() {
    UpdateFromGameObject();
    m_pAIObject->PreUpdate();
}

// Fire flags back; while aiming at a target, the turret's pan and tilt
// targets follow the tracked target and the aim point.
void CAIFilterTankObject::Update(float dt) {
    CAIObject* ai = m_pAIObject;
    ai->Update(dt);
    m_pDoodad->m_fireFlags = m_pAIObject->field_0x1A8;
    m_pDoodad->m_prevFireFlags = m_pAIObject->field_0x1AC;
    CAIObject* a = m_pAIObject;
    CAIDoodad* doodad = m_pDoodad;
    bool aiming;
    if (a->m_pTarget != NULL && (a->m_flags & 2) == 2) {
        aiming = true;
    } else {
        aiming = false;
    }
    if (aiming) {
        DoodadTank(doodad)->m_panAimTarget = *(CVector3*)&a->m_targetTrack.m_pos;
        DoodadTank(doodad)->m_tiltAimTarget = *(CVector3*)&a->m_targetPosition;
    }
    ai->field_0xC = g_aigAIFilterGlobalObject.m_time;
}

void CAIFilterTankObject::UpdateFromGameObject() {
    m_pAIObject->field_0x1AC = m_pAIObject->field_0x1A8;
    m_pAIObject->field_0x1A8 &= 3;
    CAIDoodad* doodad = m_pDoodad;
    CAITankObject* ai = (CAITankObject*)m_pAIObject;
    CAIFilterRealPosition pos;
    ((ISceneNode*)DoodadTank(doodad))->GetPosition(*(CVector3*)&pos);
    ai->SetLocation(pos);
    CAIFilterRealVector3 right;
    CAIFilterRealVector3 forward;
    CAIFilterRealVector3 up;
    ((ISceneNode*)DoodadTank(doodad))->GetRightward(*(CVector3*)&right);
    ((ISceneNode*)DoodadTank(doodad))->GetForward(*(CVector3*)&forward);
    ((ISceneNode*)DoodadTank(doodad))->GetUpward(*(CVector3*)&up);
    ai->SetCoordinateAxisDirection(right, forward, up);
    CopyMotion(ai);
    ai->m_health = ScriptHealth(m_pGameObject);
}

CAIFilterPlayerObject::CAIFilterPlayerObject() {
    m_pAIObject = new CAIPlayerObject(this);
}

CAIFilterPlayerObject::~CAIFilterPlayerObject() {
    delete m_pAIObject;
}

void CAIFilterPlayerObject::SetOneTimeVars() {
    CAIDoodad* doodad = m_pDoodad;
    CAIPlayerObject* ai = (CAIPlayerObject*)m_pAIObject;
    m_pBSObject = doodad->GetSceneNode()->GetScriptObject();
    m_pGameObject = m_pBSObject->m_gameObject;
    TriggerObjectData_struct* data = DoodadNode(doodad)->GetScriptObject()->m_trigger->m_data;
    ai->field_0x214 = 10;
    ai->field_0x14 = data->m_team;
    ai->field_0x15 = 5;
    ai->m_placedId = data->field_0x38;
    ai->m_maxHealth = data->field_0x70;
    ai->m_health = ai->m_maxHealth;
    UpdateFromGameObject();
}

void CAIFilterPlayerObject::UpdateManualUpdateVars() {}

void CAIFilterPlayerObject::PreUpdate() {
    UpdateFromGameObject();
    m_pAIObject->PreUpdate();
}

// Every 0.2 s, each AI with the player within 5 units in its field of view
// is told (event 0xE1).
void CAIFilterPlayerObject::Update(float dt) {
    CAIObject* ai = m_pAIObject;
    ai->Update(dt);
    if (g_aigAIFilterGlobalObject.m_time > m_nextSeenCheck) {
        for (CAIObject* other = CAIObject::GetGlobalListStart(); other != NULL; other = other->m_pNext) {
            if (other == ai) {
                continue;
            }
            float fov = other->m_objectFov;
            float range = 5.0f;
            if (ai->m_field_0x730) {
                range *= 0.5f;
            }
            if (ai->IsCrouching()) {
                range *= 0.75f;
            }
            if (other->IsObjectInFieldOfView(ai->m_position, range, fov, ai->IsCrouching())) {
                CAIObject* target = ai->m_pFilter->m_pAIObject->m_pTarget;
                BSObject* data;
                if (target != NULL) {
                    data = ScriptOf(target);
                } else {
                    data = NULL;
                }
                BSObjectTriggerEvent(ScriptOf(other), AIEVT_SEES_PLAYER, data, NULL, false);
            }
        }
        m_nextSeenCheck = 0.2f + g_aigAIFilterGlobalObject.m_time;
    }
    m_pDoodad->m_fireFlags = m_pAIObject->field_0x1A8;
    m_pDoodad->m_prevFireFlags = m_pAIObject->field_0x1AC;
    ai->field_0xC = g_aigAIFilterGlobalObject.m_time;
}

void CAIFilterPlayerObject::UpdateFromGameObject() {
    m_pAIObject->field_0x1AC = m_pAIObject->field_0x1A8;
    m_pAIObject->field_0x1A8 &= 3;
    CAIDoodad* doodad = m_pDoodad;
    CAIPlayerObject* ai = (CAIPlayerObject*)m_pAIObject;
    CVector3 pos;
    DoodadNode(doodad)->GetPosition(pos);
    ai->SetLocation(pos.x, pos.y, pos.z);
    CVector3 right;
    CVector3 forward;
    CVector3 up;
    DoodadNode(doodad)->GetRightward(right);
    DoodadNode(doodad)->GetForward(forward);
    DoodadNode(doodad)->GetUpward(up);
    ai->SetCoordinateAxisDirection(right.x, right.y, right.z, forward.x, forward.y, forward.z, up.x, up.y, up.z);
    CopyMotion(ai);
    ai->m_health = ScriptHealth(m_pGameObject);
}

float CAIFilterRealPosition::GetDistanceXYZReal(const CAIFilterRealPosition& other) const {
    float dy = field_0x4 - other.field_0x4;
    float dx = field_0x0 - other.field_0x0;
    float dz = field_0x8 - other.field_0x8;
    return sqrtf(dz * dz + (dx * dx + dy * dy));
}

float CAIFilterRealPosition::GetDistanceXYReal(const CAIFilterRealPosition& other) const {
    float dy = field_0x4 - other.field_0x4;
    float dx = field_0x0 - other.field_0x0;
    return sqrtf(0.0f + (dx * dx + dy * dy));
}

float CAIFilterRealPosition::GetDistanceSquaredXYZReal(const CAIFilterRealPosition& other) const {
    float dy = field_0x4 - other.field_0x4;
    float dx = field_0x0 - other.field_0x0;
    float dz = field_0x8 - other.field_0x8;
    return dz * dz + (dx * dx + dy * dy);
}

float CAIFilterRealPosition::GetDistanceSquaredXYReal(const CAIFilterRealPosition& other) const {
    float dy = field_0x4 - other.field_0x4;
    float dx = field_0x0 - other.field_0x0;
    return dx * dx + dy * dy;
}

void CAIFilterRealVector3::RotateAboutZ(float angle) {
    float s = sin(angle);
    float y = field_0x4;
    float c = cos(angle);
    float x = field_0x0;
    field_0x0 = x * c - y * s;
    field_0x4 = x * s + y * c;
}

// Yaw and pitch from these angles to the direction from `from` to `to`.
void CAIFilterRealEulerDirection::CalculateAngleDiff(const CAIFilterRealPosition& from, const CAIFilterRealPosition& to,
                                                     float* yaw, float* pitch) const {
    float v[3];
    v[0] = to.field_0x0 - from.field_0x0;
    v[1] = to.field_0x4 - from.field_0x4;
    v[2] = to.field_0x8 - from.field_0x8;
    float y = MathFunAtan2F(v[0], v[1]);
    MathFunRotateAboutZ(&v[0], &v[1], -y);
    float p = MathFunAtan2F(-v[2], v[1]);
    *yaw = MathFunNormalizeAngleNegativePiToPi(y - field_0x8);
    *pitch = MathFunNormalizeAngleNegativePiToPi(p - field_0x0);
}

void CAIFilterObject::GetTargetGuessPosition(CAIFilterRealPosition& out) {
    CAIFilterRealPosition p = m_pAIObject->m_targetPosition;
    out.field_0x0 = p.field_0x0;
    out.field_0x4 = p.field_0x4;
    out.field_0x8 = p.field_0x8;
}

BSObject* CAIFilterObject::ChooseMMGPoint() {
    TriggerObject_struct* point = m_pAIObject->ChooseMMGPoint();
    if (point != NULL) {
        return point->m_object;
    }
    return NULL;
}

bool CAIFilterObject::ShouldILeaveMMG(BSObject* gun) {
    return m_pAIObject->ShouldILeaveMMG(gun->m_trigger);
}

void CAIFilterPlayerObject::GetLookDirection(CAIFilterRealVector3& look, CAIFilterRealVector3& eye) {
    CAIDoodad* doodad = m_pDoodad;
    CVector3 forward;
    DoodadNode(doodad)->GetForward(forward);
    CVector3 pos;
    DoodadNode(doodad)->GetPosition(pos);
    CVector3 v = forward;
    look.field_0x0 = v.x;
    look.field_0x4 = v.y;
    look.field_0x8 = v.z;
    v = pos;
    eye.field_0x0 = v.x;
    eye.field_0x4 = v.y;
    eye.field_0x8 = v.z;
}

// Layout checks against the original.
typedef char AIFilterSoldierSizeCheck[sizeof(CAIFilterSoldierObject) == 0x14 ? 1 : -1];
typedef char AIFilterTankSizeCheck[sizeof(CAIFilterTankObject) == 0x14 ? 1 : -1];
typedef char AIFilterPlayerSizeCheck[sizeof(CAIFilterPlayerObject) == 0x18 ? 1 : -1];
typedef char AIFilterGlobalSizeCheck[sizeof(CAIFilterGlobal) == 0x18 ? 1 : -1];
typedef char MechanicCheckerSizeCheck[sizeof(MechanicCollisionChecker) == 0x70 ? 1 : -1];
typedef char NoiseInfoSizeCheck[sizeof(NoiseInfo) == 0xC ? 1 : -1];
