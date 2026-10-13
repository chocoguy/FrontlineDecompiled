// CAIObject: the AI state shared by soldiers, tanks and the player (as a
// target): position and axes, target tracking, vision checks, A* and spline
// path walking, cover points and obstacle avoidance. Hand-maintained.
#ifndef _GAME_AIOBJECT_H_
#define _GAME_AIOBJECT_H_

#include "types.h"
#include "game_types.h"
#include "game/AIFilter.h"
#include "game/AISplinePath.h"
#include "game/AIStatus.h"
#include "game/ObstacleAvoidance.h"
#include "game/SearchNodeList.h"

class CCoverPoint;

struct BSObject;
struct BS_STRUCT_Vector_struct;
class CAIFilterRealPosition;
class CAIFilterRealVector3;
struct CAIFilterSoundInfo;
class CAIObject;
class CAIObjectParameters;
class CAISplinePath;
struct CPathNode;
class CSearchNode;
class CSearchNodeList;
class CSearchNodeQueue;
class ISceneNode;
struct TriggerObject_struct;
struct aistatus_match;

// A critically damped follower: m_pos chases a target (and m_vel the
// target's velocity), integrated in steps of 1/120 s.
struct CAISpring {
    /* 0x00 */ CAIFilterRealVector3 m_pos;
    /* 0x10 */ CAIFilterRealVector3 m_vel;
    /* 0x20 */ f32 m_stiffness;
    /* 0x24 */ f32 m_damping;
};

// CAIObject::m_moveMode: which walk Update runs.
enum {
    AIMOVE_NONE = 0,
    AIMOVE_SPLINE = 1,          // along a spline path (m_splinePath)
    AIMOVE_ASTAR = 2,           // along an A* path to m_pathGoal
    AIMOVE_COVER_INDIRECT = 3,  // the last step into a cover point
    AIMOVE_ARBITRARY = 8,       // straight to m_movePoint
    AIMOVE_COVER = 9,           // A* to a cover point
};

// CAIObject::m_movePointType (EMovePointType). Types 3..10 are fixed points;
// the others follow the target (or where it was last seen).
enum {
    AIMOVEPOINT_CANCELLED = 3,
    AIMOVEPOINT_3D = 4,  // arrival is checked in 3D, and paths ignore the end direction
    AIMOVEPOINT_PATROL = 9,
};

// m_coverFlags: cover walk events already sent.
#define AICOVER_ARRIVED 1
#define AICOVER_NEAR 2
#define AICOVER_STEP 4

// m_flags
#define AIFLAG_FIRED 0x1      // fired this frame
#define AIFLAG_BLIND 0x8      // sees nothing
#define AIFLAG_AIM_DRIFT 0x10 // aim error follows the target's motion

// size 0x734; vtable 0x80183420
class CAIObject {
public:
    // vtable pointer at 0x0
    virtual ~CAIObject();  // 0x8004EEE0
    virtual void PreUpdate();  // 0x8004EC00
    virtual void Update(float);  // 0x8004EBFC
    virtual bool IsCrouching() const { return false; }  // 0x800478A8
    virtual bool IsPlayer() const { return false; }  // 0x800478B0
    /* 0x4 */ CAIObject* m_pNext;  // next in the global AI object list
    /* 0x8 */ CAIFilterObject* m_pFilter;  // links to the game object and its script
    /* 0xC */ f32 field_0xC;
    /* 0x10 */ f32 m_targetUpdateTime;  // when the target position was last filtered
    /* 0x14 */ u8 field_0x14;  // team (AISTATUS_TEAM; 0 = none)
    /* 0x15 */ u8 field_0x15;
    /* 0x16 */ s16 m_placedId;  // the placed object's id
    /* 0x18 */ CAIFilterRealPosition m_position;
    /* 0x28 */ CAIFilterRealPosition m_prevPosition;  // last frame's (for m_velocity)
    // Where to snap to (bit 1 of field_0x1A8 requests the snap), and the
    // facing to snap to when m_snapToAngle is set.
    /* 0x38 */ CAIFilterRealPosition m_snapPosition;
    /* 0x48 */ f32 m_snapAngle;
    /* 0x4C */ u8 m_snapToAngle;
    /* 0x4D */ u8 unk_0x4D[0x3];
    /* 0x50 */ CAIFilterRealPosition m_destination;  // where the current walk is heading
    /* 0x60 */ f32 field_0x60;  // pitch of the side axis
    /* 0x64 */ f32 field_0x64;
    /* 0x68 */ f32 m_yaw;  // of the forward axis
    /* 0x6C */ u8 unk_0x6C[0x4];
    /* 0x70 */ CAIFilterRealVector3 m_forward;
    /* 0x80 */ CAIFilterRealVector3 m_side;
    /* 0x90 */ CAIFilterRealVector3 m_up;
    /* 0xA0 */ f32 m_speed;  // |m_velocity| (units per frame)
    /* 0xA4 */ u8 unk_0xA4[0x4];
    /* 0xA8 */ CAIFilterRealVector3 m_velocity;  // per frame
    /* 0xB8 */ f32 m_bulletSpeed;  // of the current weapon
    /* 0xBC */ f32 m_invBulletSpeed;
    /* 0xC0 */ u8 m_nonAITarget;  // targeting a script object (SetNonAITarget)
    /* 0xC1 */ u8 unk_0xC1[0x3];
    /* 0xC4 */ CAIObject* m_pTarget;
    // Aim: m_targetTrack follows the target, m_targetVelocity its velocity
    // relative to ours, m_aimError a random offset scaled by distance.
    /* 0xC8 */ CAISpring m_targetTrack;
    /* 0xF0 */ CAIFilterRealPosition m_targetPosition;  // where to aim
    /* 0x100 */ CAISpring m_aimError;
    /* 0x128 */ CAISpring m_targetVelocity;
    /* 0x150 */ CAIFilterRealPosition m_lastSeenPosition;  // of the target
    /* 0x160 */ f32 m_lastSeenTime;
    /* 0x164 */ u8 unk_0x164[0x4];
    /* 0x168 */ f32 field_0x168;
    /* 0x16C */ f32 field_0x16C;
    /* 0x170 */ f32 field_0x170;
    /* 0x174 */ u8 unk_0x174[0x4];
    /* 0x178 */ f32 field_0x178;
    /* 0x17C */ f32 field_0x17C;
    /* 0x180 */ f32 field_0x180;
    /* 0x184 */ u8 unk_0x184[0x24];
    /* 0x1A8 */ u32 field_0x1A8;  // fire flags for the doodad (bit 0 fired); bit 1 requests a snap move
    /* 0x1AC */ u32 field_0x1AC;  // last frame's
    /* 0x1B0 */ u32 m_flags;  // AIFLAG_*; scripts set more (AISendFlag)
    /* 0x1B4 */ CAIStatus m_status;
    /* 0x1EC */ s32 m_numAttackers;  // AIs targeting this one
    /* 0x1F0 */ f32 field_0x1F0;
    /* 0x1F4 */ f32 field_0x1F4;
    /* 0x1F8 */ f32 m_aimTime;   // how fast the aim settles (seconds)
    /* 0x1FC */ f32 m_inaccuracy;  // aim error scale (0 = perfect)
    /* 0x200 */ u8 unk_0x200[0x4];
    /* 0x204 */ f32 field_0x204;
    /* 0x208 */ f32 m_sightRange;
    /* 0x20C */ u8 unk_0x20C[0x4];
    /* 0x210 */ f32 m_hearingRadius;
    /* 0x214 */ u32 field_0x214;
    /* 0x218 */ u32 m_moveMode;  // AIMOVE_*
    /* 0x21C */ f32 m_nextWalkUpdate;  // time of the next path/move point refresh
    /* 0x220 */ f32 m_lastDistSq;  // to m_destination at the last check
    /* 0x224 */ u8 m_approaching;  // m_lastDistSq has been decreasing
    /* 0x225 */ u8 m_skippedNode;  // already skipped a node we moved away from
    /* 0x226 */ u8 unk_0x226[0x2];
    /* 0x228 */ u32 m_coverFlags;  // AICOVER_*
    /* 0x22C */ u8 unk_0x22C[0x4];
    /* 0x230 */ CAISplinePathTraversal m_splinePath;
    /* 0x254 */ u8 unk_0x254[0x4];
    /* 0x258 */ CSearchNodeList m_path;  // A* path, start first
    /* 0x264 */ CPathNode* m_pCurNode;
    /* 0x268 */ CPathNode* m_pGoalNode;
    /* 0x26C */ CPathNode* m_pNextNode;
    /* 0x270 */ CAIObject* m_pFollowObject;  // A* walk following this object
    /* 0x274 */ u8 unk_0x274[0x4];
    /* 0x278 */ CAIFilterRealPosition m_pathGoal;
    /* 0x288 */ f32 m_nextRepathTime;
    /* 0x28C */ u8 m_pathDone;  // heading straight for m_pathGoal
    /* 0x28D */ u8 unk_0x28D[0x3];
    /* 0x290 */ CCoverPoint* m_pCoverPoint;
    /* 0x294 */ u32 m_coverType;  // COVERPOINT_TYPE claimed at m_pCoverPoint
    /* 0x298 */ CAIFilterRealPosition m_coverPosition;
    /* 0x2A8 */ CCoverPoint* m_pChosenCoverPoint;  // ChooseCoverPoint's result
    /* 0x2AC */ u8 m_checkCoverUseful;
    /* 0x2AD */ u8 unk_0x2AD[0x3];
    /* 0x2B0 */ CAIFilterRealPosition m_peekPosition;
    /* 0x2C0 */ u8 m_atCoverPoint;
    /* 0x2C1 */ u8 unk_0x2C1[0x3];
    /* 0x2C4 */ f32 m_nextCoverCheckTime;
    /* 0x2C8 */ CAIFilterRealPosition m_movePoint;
    /* 0x2D8 */ s32 m_movePointType;  // EMovePointType (AIMOVEPOINT_*)
    /* 0x2DC */ f32 m_moveArriveDist;
    /* 0x2E0 */ BS_STRUCT_Vector_struct* m_pMovePointVar;  // script vector kept up to date
    /* 0x2E4 */ bool m_avoidObstacles;
    /* 0x2E5 */ u8 unk_0x2E5[0x3];
    /* 0x2E8 */ CObstacleAvoidance m_avoidance;
    /* 0x688 */ CAIFilterRealPosition m_avoidPosition;
    // Vision modes: a single object, or every object matching a status list
    // (checked one per call, m_pVisionCursor remembering where).
    /* 0x698 */ CAIObject* m_pVisionCursor;
    /* 0x69C */ CAIObject* m_pVisionObject;
    /* 0x6A0 */ u8 unk_0x6A0[0x10];
    /* 0x6B0 */ CAIFilterRealPosition m_seekPosition;  // vision seeking: look around here
    /* 0x6C0 */ CAIFilterRealVector3 m_seekSide;
    /* 0x6D0 */ f32 m_seekOffsetSide;
    /* 0x6D4 */ f32 m_seekOffsetUp;
    /* 0x6D8 */ u8 m_targetVisible;
    /* 0x6D9 */ u8 unk_0x6D9[0x3];
    /* 0x6DC */ f32 m_listSightRange;
    /* 0x6E0 */ f32 m_objectSightRange;
    /* 0x6E4 */ f32 m_corpseSightRange;
    /* 0x6E8 */ f32 m_objectFov;  // radians
    /* 0x6EC */ f32 m_listFov;
    /* 0x6F0 */ f32 m_corpseFov;
    /* 0x6F4 */ aistatus_match m_visionMatches[4];
    /* 0x724 */ int m_numVisionMatches;
    /* 0x728 */ u32 m_aimHeightMode;  // where on the target to aim
    /* 0x72C */ s32 m_aimHeightChoice;  // random pick for some modes (0 = not yet)
    /* 0x730 */ bool m_field_0x730;  // halves the sight range at which this object is noticed
    /* 0x731 */ u8 m_targetSeen;  // the last known target position (0x150) is valid

    bool ShouldILeaveMMG(TriggerObject_struct*);  // 0x800478B8
    TriggerObject_struct* ChooseMMGPoint();  // 0x80047B04
    static void PropagateSoundToAllObjects(const CAIFilterSoundInfo&, CAIObject*);  // 0x80047E0C
    bool IsObjectReachable(const CAIFilterRealPosition&);  // 0x800480A4
    CAIObject* CheckVisionSeekingMode();  // 0x80048134
    void SetupVisionSeekingMode(CAIObject*, CAIFilterRealPosition&, float);  // 0x80048270
    bool IsObjectInLineOfSight(CAIFilterRealPosition&, float, bool);  // 0x80048408
    CAIObject* CheckLineOfSightModeList();  // 0x80048564
    CAIObject* CheckLineOfSightModeObject();  // 0x800487B0
    bool IsTargetInLineOfSight(float);  // 0x800489C4
    void SetupLineOfSightModeList(aistatus_match*, int, float);  // 0x80048E10
    void SetupLineOfSightModeObject(CAIObject*, float);  // 0x80048E98
    bool IsObjectInFieldOfView(CAIFilterRealPosition&, float, float, bool);  // 0x80048EEC
    CAIObject* CheckFieldOfViewCorpse();  // 0x80049288
    int GetAIStatusVar(aistatus_var) const;  // 0x80049380
    CAIObject* CheckFieldOfViewModeList();  // 0x80049974
    CAIObject* CheckFieldOfViewModeObject();  // 0x80049AF4
    void SetupFieldOfViewCorpse(float, float);  // 0x80049BFC
    void SetupFieldOfViewModeList(aistatus_match*, int, float, float);  // 0x80049C90
    void SetupFieldOfViewModeObject(CAIObject*, float, float);  // 0x80049D4C
    void ProcessObstacleAvoidance();  // 0x80049DD4
    void EngageObstacleAvoidance(bool, float, float, float);  // 0x80049E28
    void WalkToArbitraryPoint();  // 0x80049E88
    void StartWalkToArbitraryPoint(CAIFilterRealPosition&);  // 0x80049FD4
    void SetupArbitraryPointUpdate(EMovePointType, BS_STRUCT_Vector_struct*, float);  // 0x8004A0DC
    void WalkToCoverPointIndirect();  // 0x8004A0EC
    void WalkToCoverPointDirect();  // 0x8004A284
    void WalkToCoverPoint();  // 0x8004A454
    void CheckCoverpointUsefulness();  // 0x8004AA28
    bool StartCoverPointWalkToStored(bool);  // 0x8004AB04
    void CommitCoverPointWalkToStored();  // 0x8004AB24
    bool StartCoverPointWalk(bool);  // 0x8004ABE0
    void SetCoverPoint(int, bool);  // 0x8004AEC4
    CCoverPoint* ChooseCoverPoint(ECoverPointSelectionType, const ISceneNode*, float, float);  // 0x8004B000
    CCoverPoint* StoreClosestCoverPoint();  // 0x8004B170
    bool ComputeAStarPath(CPathNode*, const CPathNode*, float, CSearchNodeQueue&, CSearchNodeList&);  // 0x8004B2C0
    CPathNode* GetNextPathNode(CPathNode*, CPathNode*);  // 0x8004B5B4
    void CleanupAStarPathWalk();  // 0x8004B728
    CSearchNode* AddChildrenToSearchNodeQueue(CSearchNode*, const CPathNode*, CSearchNodeQueue&, CSearchNodeList&);  // 0x8004B784
    void WalkAStarPath();  // 0x8004B8FC
    void UpdateAStarPathWalk();  // 0x8004B988
    bool StartAStarPathWalk();  // 0x8004C218
    bool SetupAStarPathWalkForPatrol(const CAIFilterRealVector3&);  // 0x8004C490
    bool SetupAStarPathWalk(const CAIFilterRealVector3&, const CAIFilterRealVector3&, bool);  // 0x8004C720
    bool StartAStarPathWalk(CAIObject*);  // 0x8004C9A4
    void WalkNextSplinePathPoint();  // 0x8004CD34
    void StartSplinePathWalkForward(CAISplinePath*);  // 0x8004CD80
    int ComputeStatusIsEnemy(CAIObject*);  // 0x8004CE7C
    int ComputeStatusLOSValue(CAIObject*);  // 0x8004CF48
    int ComputeStatusThreatValue(CAIObject*);  // 0x8004D0FC
    int ComputeStatusExponentialDistance(CAIObject*);  // 0x8004D164
    int ComputeStatusDistance(CAIObject*);  // 0x8004D2BC
    int ComputeStatusVisibility(CAIObject*);  // 0x8004D310
    void SetNonAITarget(BSObject*);  // 0x8004D318
    CAIObject* SetTarget(CAIObject*);  // returns the previous target  // 0x8004D41C
    float GetDistanceSquaredXYReal(CAIObject*);  // 0x8004D53C
    float GetDistanceSquaredXYZReal();  // to the aim point  // 0x8004D564
    float GetDistanceSquaredXYZReal(CAIObject*);  // 0x8004D590
    float GetDistanceXYZReal(CAIObject*);  // 0x8004D5B8
    void GetCoordinateAxisDirection(CAIFilterRealVector3*, CAIFilterRealVector3*, CAIFilterRealVector3*);  // 0x8004D5E0
    void SetCoordinateAxisDirection(CAIFilterRealVector3&, CAIFilterRealVector3&, CAIFilterRealVector3&);  // 0x8004D614
    void SetCoordinateAxisDirection(float, float, float, float, float, float, float, float, float);  // 0x8004D6BC
    void SetLocation(CAIFilterRealPosition&);  // 0x8004D75C
    void SetLocation(float, float, float);  // 0x8004D778
    float PerformCollisionTest(ECollisionTestDirection, float);  // 0x8004D788
    void CalculateTargetPosition();  // 0x8004DB08
    void UpdateTargetPosition();  // 0x8004E024
    float TestGrenadeShot(float);  // launch speed, or -1 if no arc reaches the target  // 0x8004E9F8
    void Init();  // 0x8004EBE0
    bool UpdateReactionTime();  // 0x8004EC04
    static void ResetGlobalList();  // 0x8004EC60
    static CAIObject* GetGlobalListStart();  // 0x8004EC70
    void RemovePointersTo(CAIObject*);  // 0x8004EC78
    void RemoveFromGlobalList();  // 0x8004EE44
    CAIObject();  // 0x8004F024

    // The script of the game object (through the filter).
    BSObject* GetScript() const;
};

// size >= 0x0 (from field accesses)
class CAIObjectParameters {
public:
    ~CAIObjectParameters();  // 0x8004F130
};

extern int g_iAIObjectCount;  // 0x8034E8E8

// Health as an AI status level from 1 to 15 (0 only when dead).
inline int AIHealthLevel(float health, float maxHealth) {
    return (int)(15.0f * (health / maxHealth) + 0.999f);
}

// Teams: 0 is hostile to 1 and 2; 1 and 2 are hostile to each other.
inline bool AIIsEnemyTeam(int mine, int theirs) {
    switch (mine) {
    case 0:
        switch (theirs) {
        case 0:
            return false;
        case 1:
        case 2:
            return true;
        }
        return false;
    case 1:
        switch (theirs) {
        case 0:
        case 1:
            return false;
        case 2:
            return true;
        }
        return false;
    case 2:
        switch (theirs) {
        case 0:
        case 2:
            return false;
        case 1:
            return true;
        }
        return false;
    }
    return false;
}

inline BSObject* CAIObject::GetScript() const {
    return ((CAIFilterSoldierObject*)m_pFilter)->m_pBSObject;
}

#endif
